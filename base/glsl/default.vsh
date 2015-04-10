
uniform mat4 rpModelView;
uniform mat4 rpProjection;

/*
out vs_output
{
  vec4 position;
  vec4 texcoord;
} result;
*/

void main(void)
{
  gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = rpProjection * rpModelView * gl_Vertex;
}