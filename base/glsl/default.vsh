
/*
out vs_output
{
  vec4 position;
  vec4 texcoord;
} result;
*/
void main(void)
{
  //result.position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;
  //result.texcoord = gl_MultiTexCoord0; 
  gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;
}