#include "RenderProgram.h"

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

/*
=================
R_PreprocessShader
=================
*/
static bool R_PreprocessShader( char* src, int srcsize, char* dest, int destsize ) {
	static const char* const inc_stmt = "#include ";

	char* inc_start = strstr( src, inc_stmt );
	if (!inc_start) {
		if (srcsize >= destsize) {
			common->Warning( ": File too large\n" );
			return false;
		}

		memcpy( dest, src, srcsize );
		dest[srcsize] = '\0';

		return true;
	}

	char* filename_start = strstr( inc_start, "\"" );
	if (!filename_start)
		return false;

	filename_start++;

	char* filename_stop = strstr( filename_start, "\"" );
	if (!filename_stop)
		return false;

	int filename_len = (ptrdiff_t)filename_stop - (ptrdiff_t)filename_start;
	filename_stop++;

	int bytes_before_inc = (ptrdiff_t)inc_start - (ptrdiff_t)src;
	int bytes_after_inc = (ptrdiff_t)srcsize - ((ptrdiff_t)filename_stop - (ptrdiff_t)src);

	idStr fullPath = idStr( "glsl/" ) + idStr( filename_start, 0, filename_len );

	char	*fileBuffer = nullptr;
	fileSystem->ReadFile( fullPath.c_str(), (void **)&fileBuffer, NULL );
	if (!fileBuffer) {
		common->Printf( ": File not found\n" );
		return false;
	}
	int file_size = strlen( fileBuffer );

	if (file_size + bytes_before_inc + bytes_after_inc >= destsize) {
		common->Printf( ": File too large\n" );
		fileSystem->FreeFile( fileBuffer );
		return false;
	}

	memcpy( dest, src, bytes_before_inc );
	dest += bytes_before_inc;
	memcpy( dest, fileBuffer, file_size );
	dest += file_size;
	memcpy( dest, filename_stop, bytes_after_inc );
	dest[bytes_after_inc] = '\0';
	return true;
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

	// load the program even if we don't support it, so
	// fs_copyfiles can generate cross-platform data dumps
	// 
	char	*fileBuffer = nullptr;
	fileSystem->ReadFile( fullPath.c_str(), (void **)&fileBuffer, NULL );
	if (!fileBuffer) {
		common->Printf( ": File not found\n" );
		return 0;
	}

	const int buffer_size = 1024 * 256;
	char* buffer = new char[buffer_size];

	bool ok = R_PreprocessShader( fileBuffer, strlen( fileBuffer ), buffer, buffer_size );

	fileSystem->FreeFile( fileBuffer );

	if (!ok) {
		return 0;
	}

	GLuint shaderObject = glCreateShader( shaderType );

	const int bufferLen = strlen( buffer );

	glShaderSource( shaderObject, 1, (const GLchar**)&buffer, &bufferLen );
	glCompileShader( shaderObject );

	delete buffer;

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