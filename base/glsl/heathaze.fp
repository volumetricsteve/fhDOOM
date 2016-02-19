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

float linearDepth()
{
  float ndcDepth = (2.0 * gl_FragCoord.z - gl_DepthRange.near - gl_DepthRange.far) / (gl_DepthRange.far - gl_DepthRange.near);
  return ndcDepth / gl_FragCoord.w;
}

float getDistanceScale()
{
  float maxDistance = 800;

  float depth = linearDepth();
  depth = clamp(depth, 0, maxDistance);

  return (maxDistance - depth)/maxDistance;
}

vec4 test4()
{
  float distanceScaleMagnitude = getDistanceScale();  

  vec2 magnitude = shaderParm1.xy * 0.008;
  vec2 scroll = shaderParm0.xy * 0.3;

  vec2 uv = gl_FragCoord.xy * (1.0 / rpCurrentRenderSize.zw);
  
  vec2 uvDist = uv + scroll;

  vec4 distortion = (texture2D(texture1, uvDist * 2.5) - 0.5) * 2.0;
  
  uv += distortion.wy * magnitude * distanceScaleMagnitude;  

  return texture2D(texture0, fixScreenTexCoord(uv));
}

void main(void)
{
  result = test4();
}