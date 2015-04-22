#include "global.inc"

in vs_output
{
  vec4 color;
  vec3 cubecoord;
  vec3 normal;
  vec2 texNormal;
} frag;

out vec4 result;

// texture 0 is the cube map
// texture 1 is the per-surface bump map
// texture 2 is the light falloff texture
// texture 3 is the light projection texture
// texture 4 is the per-surface diffuse map
// texture 5 is the per-surface specular map
// texture 6 is the specular lookup table


vec3 reflect(vec3 I, vec3 N)
{
  return I - 2.0 * dot(N, I) * N;  
}

void main(void)
{  
  //vec3 bump = normalize(2.0 * texture2D(texture2, frag.texNormal.st).agb - 1.0);

  vec3 r = reflect(frag.cubecoord, normalize(frag.normal));

  result = texture(cubemap1, r);
}