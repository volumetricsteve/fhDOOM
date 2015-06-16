#include "global.inc"

in vs_output
{
  vec4 color;
  vec3 cubecoord;

  vec3 normal;
  vec3 tangent;
  vec3 binormal;

  vec2 texcoord;
} frag;

out vec4 result;

vec3 reflect(vec3 I, vec3 N)
{
  return I - 2.0 * dot(N, I) * N;  
}

void main(void)
{ 
  vec3 localNormal = 2.0 * texture2D(texture2, frag.texcoord.st).agb - 1.0;  

  vec3 normal = normalize(frag.normal);
  vec3 tangent = normalize(frag.tangent);    
  vec3 binormal = normalize(frag.binormal);    
  mat3 TBN = mat3(tangent, binormal, normal);  

  vec3 r = reflect(frag.cubecoord, TBN * localNormal);
  result = texture(cubemap1, r) * frag.color * rpDiffuseColor;
}