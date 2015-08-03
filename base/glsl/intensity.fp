#include "global.inc"

layout(location = TEXTURE_UNIT_0) uniform sampler2D texture1;

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

vec2 fixScreenTexCoord(vec2 st)
{
  float x = rpCurrentRenderSize.z / rpCurrentRenderSize.x;
  float y = rpCurrentRenderSize.w / rpCurrentRenderSize.y;
  return st * vec2(x, y);  
}

void main(void)
{  
  vec4 color = texture2D(texture1, fixScreenTexCoord(frag.texcoord));
  float j = color.r;
  if(color.g > j)
    j = color.g;
  if(color.b > j)
    j = color.b;

  if(j < 0.5)
  {
    result.r = 2.0 * (0.5 - j);
    result.g = 2.0 * j;
    result.b = 0;
  } 
  else
  {
    result.r = 0;
    result.g = 2.0 * (1 - j);
    result.b = 2.0 * (j - 0.5);    
  }
  result.a = 1.0f;
}