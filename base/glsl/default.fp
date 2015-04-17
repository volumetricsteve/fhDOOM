#include "global.inc"

in vs_output
{
  vec4 color;
  vec2 texcoord;
} frag;

out vec4 result;

// texture 0 is the cube map
// texture 1 is the per-surface bump map
// texture 2 is the light falloff texture
// texture 3 is the light projection texture
// texture 4 is the per-surface diffuse map
// texture 5 is the per-surface specular map
// texture 6 is the specular lookup table


void main(void)
{  
  result = texture2D(texture1, frag.texcoord) * frag.color * rpDiffuseColor;
}