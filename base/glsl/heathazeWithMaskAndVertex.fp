#include "global.inc"

layout(location = TEXTURE_UNIT_0) uniform sampler2D texture0;
layout(location = TEXTURE_UNIT_1) uniform sampler2D texture1;
layout(location = TEXTURE_UNIT_2) uniform sampler2D texture2;

in vs_output
{
  vec4 color;
  vec2 texcoord;
  vec4 texcoord1;
  vec4 texcoord2;  
} frag;

out vec4 result;

vec2 fixScreenTexCoord(vec2 st)
{
  float x = rpCurrentRenderSize.z / rpCurrentRenderSize.x;
  float y = rpCurrentRenderSize.w / rpCurrentRenderSize.y;
  return st * vec2(x, y);  
}

float linearDepth()
{
  float ndcDepth = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.far - gl_DepthRange.near);
  return ndcDepth / gl_FragCoord.w;
}

vec4 mad_sat(vec4 a, vec4 b, vec4 c)
{
  return clamp(a * b + c, 0, 1);
}

void main(void)
{
  // load the distortion map
  vec4 mask = texture2D(texture2, frag.texcoord);
  mask *=  frag.color;
  // kill the pixel if the distortion wound up being very small
  
  mask.xy -= 0.01f;
  if(mask.x < 0 || mask.y < 0)
    discard;

  // load the filtered normal map and convert to -1 to 1 range
  vec4 localNormal = texture2D( texture1, frag.texcoord1.xy );
  localNormal.x = localNormal.a;
  localNormal = localNormal * 2 - 1;
  localNormal = localNormal * mask;

  // calculate the screen texcoord in the 0.0 to 1.0 range
  vec4 screenTexCoord = vec4(gl_FragCoord.x/rpCurrentRenderSize.z, gl_FragCoord.y/rpCurrentRenderSize.w, 0, 0); //vposToScreenPosTexCoord( fragment.position.xy );

  screenTexCoord = mad_sat(localNormal, frag.texcoord2, screenTexCoord);
/*
  screenTexCoord += ( localNormal * frag.texcoord2.xy );
  screenTexCoord = clamp(screenTexCoord, 0, 1);
*/
  result = texture2D( texture0, fixScreenTexCoord(screenTexCoord.xy));  
  
}