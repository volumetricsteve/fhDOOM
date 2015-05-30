#include "global.inc"

in vs_output
{
  vec2 texcoord0;
  vec2 texcoord1;
} frag;

out vec4 result;

void main(void)
{
  result = texture2D( texture0, frag.texcoord0 ) * vec4(rpDiffuseColor.rgb, 1.0);
/*
  result.r = frag.texcoord0.x;
  result.g = frag.texcoord0.y;
  result.b = 0;
  result.g = 1;  
*/  
}