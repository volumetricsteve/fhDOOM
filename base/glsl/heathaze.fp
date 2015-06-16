#include "global.inc"

layout(location = TEXTURE_UNIT_0) uniform sampler2D texture0;
layout(location = TEXTURE_UNIT_1) uniform sampler2D texture1;

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

vec4 test2()
{
  vec2 magnitude = 20.0 / shaderParm1.xy;// * linearDepth()/300;
  vec2 frequency = shaderParm0.xy / 100.0;// * linearDepth()/300;

  vec2 uv = gl_FragCoord.xy * (1.0 / rpCurrentRenderSize.zw);
  
  vec2 uvDist = vec2(uv.xy);

  uvDist.x = uvDist.x + 64 * sin(frequency.x);
  uvDist.y = uvDist.y + 64 * sin(frequency.y);
  
  vec4 distortionColor = texture2D(texture1, uvDist) - vec4(0.5,0.5,0.5,0.5);  
  
  uv.x = uv.x + distortionColor.w / magnitude.x;
  uv.y = uv.y + distortionColor.y / magnitude.y;
  

  return texture2D(texture0, fixScreenTexCoord(uv)) ;
}


void main(void)
{
  result = test2();
}