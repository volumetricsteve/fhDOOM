#include "global.inc"

layout(binding = 0) uniform sampler2D texture0;
layout(binding = 1) uniform sampler2D texture1;

in vs_output
{
  vec4 color;
  vec2 texcoord;
} frag;

out vec4 result;

vec2 fixScreenTexCoord(vec2 st)
{
  float x = rpCurrentRenderSize.z / rpCurrentRenderSize.x;
  float y = rpCurrentRenderSize.w / rpCurrentRenderSize.y;
  return st * vec2(x, y);  
}

void main(void)
{
  vec2 screenTexCoord = frag.texcoord;
  
  // compute warp factor
  vec4 warpFactor = 1.0 - ( texture2D( texture1, screenTexCoord.xy ) * frag.color );
  screenTexCoord -= vec2( 0.5, 0.5 );
  screenTexCoord *= warpFactor.xy;
  screenTexCoord += vec2( 0.5, 0.5 );

  // load the screen render
  result = texture2D( texture0, screenTexCoord );  
}