#include "global.inc"

layout(location = TEXTURE_UNIT_0) uniform sampler2D texture0;

out vec4 result;

vec2 fixScreenTexCoord(vec2 st)
{
  float x = rpCurrentRenderSize.z / rpCurrentRenderSize.x;
  float y = rpCurrentRenderSize.w / rpCurrentRenderSize.y;
  return st * vec2(x, y);  
}

void main(void)
{
  vec2 uv = gl_FragCoord.xy / rpCurrentRenderSize.zw;
  vec4 src = texture2D(texture0, fixScreenTexCoord(uv));

  vec4 target = shaderParm1 * dot( vec3( 0.333, 0.333, 0.333 ), src.xyz );
  result = src * (1-shaderParm0.x) + target * shaderParm0.x;

//  result = vec4(1,0,0,1);
}