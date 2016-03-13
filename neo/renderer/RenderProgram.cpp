#include "RenderProgram.h"
#include "../idlib/StrRef.h"

static GLuint currentProgram = 0;

#define MAX_GLPROGS 128
static fhRenderProgram glslPrograms[MAX_GLPROGS];// = { 0 };

const fhRenderProgram* shadowProgram = nullptr;
const fhRenderProgram* interactionProgram = nullptr;
const fhRenderProgram* depthProgram = nullptr;
const fhRenderProgram* shadowmapProgram = nullptr;
const fhRenderProgram* defaultProgram = nullptr;
const fhRenderProgram* depthblendProgram = nullptr;
const fhRenderProgram* skyboxProgram = nullptr;
const fhRenderProgram* bumpyEnvProgram = nullptr;
const fhRenderProgram* fogLightProgram = nullptr;
const fhRenderProgram* blendLightProgram = nullptr;
const fhRenderProgram* vertexColorProgram = nullptr;
const fhRenderProgram* flatColorProgram = nullptr;
const fhRenderProgram* intensityProgram = nullptr;

class fhParseException {
public:
	idStr filename;
	idStr message;
	int line;
	int column;

	fhParseException(const idStr& filename, int line, int column, const char* message) 
		: filename(filename)
		, message(message)
		, line(line)
		, column(column) {
	}
};

class fhFileNotFoundException {
public:
	idStr filename;

	explicit fhFileNotFoundException( const idStr& filename )
		: filename( filename ) {
	}
};

static idStr R_ReadFile(const char* filename) {
	char	*fileBuffer = nullptr;
	fileSystem->ReadFile( filename, (void **)&fileBuffer, NULL );
	if (!fileBuffer) {
		common->Printf( ": File not found\n" );
		throw fhFileNotFoundException(filename);
	}

	idStr ret = fileBuffer;
	fileSystem->FreeFile( fileBuffer );	
	return ret;
}

static idStr R_LoadPreprocessed( const idStr& filename, idList<idStr>& previoulsyLoadedFiles, idList<idStr>& includeStack )
{
	includeStack.Append( filename );
	previoulsyLoadedFiles.Append( filename );

	const int fileIndex = previoulsyLoadedFiles.Num() - 1;
	
	idStr content = R_ReadFile(filename.c_str());
	idStr ret;

	fhStrRef ptr = fhStrRef( content.c_str(), content.Length() );
	fhStrRef remaining = ptr;

	int currentLine = 1;
	int currentColumn = 1;
	bool isLineComment = false;

	for (; !ptr.IsEmpty(); ++ptr)
	{
		if (ptr[0] == '\n')
		{
			++currentLine;
			currentColumn = 1;
			isLineComment = false;
			continue;
		}

		if (isLineComment)
		{
			continue;
		}

		if (ptr.StartsWith( "//" ))
		{
			isLineComment = true;
			continue;
		}

		static const fhStrRef includeDirective = "#include \"";
		if (currentColumn == 1 && ptr.StartsWith( includeDirective ))
		{
			fhStrRef includeFilename = ptr.Substr( includeDirective.Length() );
			for (int i = 0; i < includeFilename.Length() + 1; ++i)
			{
				if (i == includeFilename.Length())
					throw fhParseException( filename, currentLine, currentColumn, "unexpected end-of-file in preprocessor include" );

				if (includeFilename[i] == '\n')
					throw fhParseException( filename, currentLine, currentColumn, "unexpected end-of-line in preprocessor include" );

				if (includeFilename[i] == '"')
				{
					includeFilename = includeFilename.Substr( 0, i );
					break;
				}
			}

			if (includeFilename.IsEmpty())
				throw fhParseException( filename, currentLine, currentColumn, "empty filename in preprocessor include" );

			if (includeStack.FindIndex( includeFilename.ToString() ) >= 0)
				throw fhParseException( filename, currentLine, currentColumn, "circular preprocessor include" );

			idStr includeContent;
			//try to load included shader relative to current file. If that fails try to load included shader from root directory.
			try
			{
				idStr includeFilePath;
				filename.ExtractFilePath(includeFilePath);				
				includeFilePath.AppendPath( includeFilename.c_str(), includeFilename.Length() );

				includeContent = R_LoadPreprocessed( includeFilePath, previoulsyLoadedFiles, includeStack );
				ret.Append( remaining.c_str(), ptr.c_str() - remaining.c_str() );
				ret.Append( includeContent );
				//ret.Append( "\n#line " + toString( currentLine + 1 ) + " \"" + filename + "\"" );
			}
			catch (const fhFileNotFoundException& e)
			{				
				try
				{
					includeContent = R_LoadPreprocessed( includeFilename.ToString(), previoulsyLoadedFiles, includeStack );
					ret.Append( remaining.c_str(), ptr.c_str() - remaining.c_str() );
					ret.Append( includeContent );
					//ret.append( "\n#line " + ToString( currentLine + 1 ) + " \"" + filename + "\"" );
				}
				catch (const fhFileNotFoundException& e)
				{
					throw fhParseException( filename, currentLine, currentColumn, idStr( "include file not found: " ) + includeFilename.ToString() );
				}
			}

			//skip rest of the line
			while (!ptr.IsEmpty() && ptr[0] != '\n')
			{
				++ptr;
			}

			++currentLine;
			currentColumn = 1;
			remaining = ptr;

			continue;
		}

		currentColumn++;
	}

	ret.Append( remaining.ToString() );
	includeStack.RemoveIndex(includeStack.Num() - 1);
	return ret;
}


/*
=================
R_LoadGlslShader
=================
*/
static GLuint R_LoadGlslShader( GLenum shaderType, const char* filename ) {
	assert( filename );
	assert( filename[0] );
	assert( shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER );

	idStr	fullPath = "glsl/";
	fullPath += filename;

	if (shaderType == GL_VERTEX_SHADER)
		common->Printf( "load vertex shader %s\n", fullPath.c_str() );
	else
		common->Printf( "load fragment shader %s\n", fullPath.c_str() );

	try {
		idList<idStr> files;
		idList<idStr> stack;
		idStr shader = R_LoadPreprocessed( fullPath, files, stack );

		GLuint shaderObject = glCreateShader( shaderType );
		const int len = shader.Length();
		const char* ptr = shader.c_str();
		glShaderSource( shaderObject, 1, (const GLchar**)&ptr, &len );
		glCompileShader( shaderObject );

		GLint success = 0;
		glGetShaderiv( shaderObject, GL_COMPILE_STATUS, &success );
		if (success == GL_FALSE) {
			char buffer[1024];
			GLsizei length;
			glGetShaderInfoLog( shaderObject, sizeof(buffer)-1, &length, &buffer[0] );
			buffer[length] = '\0';
			common->Printf( "failed to compile shader '%s': %s", filename, &buffer[0] );
			return 0;
		}

		return shaderObject;
	}
	catch (const fhFileNotFoundException& e) {
		return 0;
	}
	catch (const fhParseException& e) {
		return 0;
	}

	return 0;
}

namespace {
	// Little RAII helper to deal with detaching and deleting of shader objects
	struct GlslShader {
		GlslShader( GLuint ident )
			: ident( ident )
			, program( 0 )
		{}

		~GlslShader() {
			release();
		}

		void release() {
			if (ident) {
				if (program) {
					glDetachShader( program, ident );
				}
				glDeleteShader( ident );
			}

			ident = 0;
			program = 0;
		}

		void attachToProgram( GLuint program ) {
			this->program = program;
			glAttachShader( program, ident );
		}

		GLuint ident;
		GLuint program;
	};
}


/*
=================
R_FindGlslProgram
=================
*/
const fhRenderProgram* R_FindGlslProgram( const char* vertexShaderName, const char* fragmentShaderName ) {
	assert( vertexShaderName && vertexShaderName[0] );
	assert( fragmentShaderName && fragmentShaderName[0] );

	const int vertexShaderNameLen = strlen( vertexShaderName );
	const int fragmentShaderNameLen = strlen( fragmentShaderName );

	int i;
	for (i = 0; i < MAX_GLPROGS; ++i) {
		const int vsLen = strlen( glslPrograms[i].vertexShader() );
		const int fsLen = strlen( glslPrograms[i].fragmentShader() );

		if (!vsLen || !fsLen)
			break;

		if (vsLen != vertexShaderNameLen || fsLen != fragmentShaderNameLen)
			continue;

		if (idStr::Icmpn( vertexShaderName, glslPrograms[i].vertexShader(), vsLen ) != 0)
			continue;

		if (idStr::Icmpn( fragmentShaderName, glslPrograms[i].fragmentShader(), fsLen ) != 0)
			continue;

		return &glslPrograms[i];
	}

	if (i >= MAX_GLPROGS) {
		common->Error( "cannot create GLSL program, maximum number of programs reached" );
		return nullptr;
	}

	glslPrograms[i].Load(vertexShaderName, fragmentShaderName);

	return &glslPrograms[i];
}


fhRenderProgram::fhRenderProgram()
: ident(0) {
	vertexShaderName[0] = '\0';
	fragmentShaderName[0] = '\0';
}

fhRenderProgram::~fhRenderProgram() {
}

void fhRenderProgram::Load( const char* vs, const char* fs ) {
	const int vsLen = min( strlen( vs ), sizeof(vertexShaderName)-1 );
	const int fsLen = min( strlen( fs ), sizeof(fragmentShaderName)-1 );
	strncpy( vertexShaderName, vs, vsLen );
	strncpy( fragmentShaderName, fs, fsLen );
	vertexShaderName[vsLen] = '\0';
	fragmentShaderName[fsLen] = '\0';
	Load();
}

void fhRenderProgram::Load() {
	if (!vertexShaderName[0] || !fragmentShaderName[0]) {
		return;
	}

	GlslShader vertexShader = R_LoadGlslShader( GL_VERTEX_SHADER, vertexShaderName );
	if (!vertexShader.ident) {
		common->Warning( "failed to load GLSL vertex shader: %s", vertexShaderName );
		return;
	}

	GlslShader fragmentShader = R_LoadGlslShader( GL_FRAGMENT_SHADER, fragmentShaderName );
	if (!fragmentShader.ident) {
		common->Warning( "failed to load GLSL fragment shader: %s", fragmentShaderName );
		return;
	}

	const GLuint program = glCreateProgram();
	if (!program) {
		common->Warning( "failed to create GLSL program object" );
		return;
	}

	vertexShader.attachToProgram( program );
	fragmentShader.attachToProgram( program );
	glLinkProgram( program );

	GLint isLinked = 0;
	glGetProgramiv( program, GL_LINK_STATUS, &isLinked );
	if (isLinked == GL_FALSE) {

		char buffer[1024];
		GLsizei length;
		glGetProgramInfoLog( program, sizeof(buffer)-1, &length, &buffer[0] );
		buffer[length] = '\0';

		vertexShader.release();
		fragmentShader.release();
		glDeleteProgram( program );

		common->Warning( "failed to link GLSL shaders to program: %s", &buffer[0] );
		return;
	}

	if (ident) {
		glDeleteProgram( ident );
	}

	ident = program;

}

void fhRenderProgram::Reload() {
	Purge();
	Load();
}

void fhRenderProgram::Purge() {
	if (ident) {
		glDeleteProgram( ident );
	}
	
	ident = 0;
}

void fhRenderProgram::Bind() const {
	if(currentProgram != ident) {
		glUseProgram( ident );
		currentProgram = ident;
	}
}

void fhRenderProgram::Unbind() {
	if(currentProgram) {
		glUseProgram( 0 );
		currentProgram = 0;
	}	
}

const char* fhRenderProgram::vertexShader() const {
	return &vertexShaderName[0];
}

const char* fhRenderProgram::fragmentShader() const {
	return &fragmentShaderName[0];
}

void fhRenderProgram::ReloadAll() {
	Unbind();

	for (int i = 0; i < MAX_GLPROGS; ++i) {
		glslPrograms[i].Reload();
	}
}

void fhRenderProgram::PurgeAll() {
	Unbind();

	for (int i = 0; i < MAX_GLPROGS; ++i) {
		glslPrograms[i].Purge();
	}
}

void fhRenderProgram::Init() {
	fogLightProgram = R_FindGlslProgram( "fogLight.vp", "fogLight.fp" );
	blendLightProgram = R_FindGlslProgram( "blendLight.vp", "blendLight.fp" );
	shadowProgram = R_FindGlslProgram( "shadow.vp", "shadow.fp" );
	depthProgram = R_FindGlslProgram( "depth.vp", "depth.fp" );
	shadowmapProgram = R_FindGlslProgram( "shadowmap.vp", "shadowmap.fp" );
	defaultProgram = R_FindGlslProgram( "default.vp", "default.fp" );
	depthblendProgram = R_FindGlslProgram( "depthblend.vp", "depthblend.fp" );
	skyboxProgram = R_FindGlslProgram( "skybox.vp", "skybox.fp" );
	bumpyEnvProgram = R_FindGlslProgram( "bumpyenv.vp", "bumpyenv.fp" );
	interactionProgram = R_FindGlslProgram( "interaction.vp", "interaction.fp" );
	vertexColorProgram = R_FindGlslProgram( "vertexcolor.vp", "vertexcolor.fp" );
	flatColorProgram = R_FindGlslProgram( "flatcolor.vp", "flatcolor.fp" );
	intensityProgram = R_FindGlslProgram( "intensity.vp", "intensity.fp" );
}