#include "global.inc"

layout(location = TEXTURE_UNIT_0) uniform sampler2D texture0;
layout(location = TEXTURE_UNIT_1) uniform sampler2D texture1;

in vs_output
{
  vec4 texcoord0;
  vec2 texcoord1;
} frag;

out vec4 result;

void main(void)
{
  result = textureProj( texture0, frag.texcoord0.xyw ) * texture2D( texture1, frag.texcoord1 ) * vec4(rpDiffuseColor.rgb, 1.0); 
}