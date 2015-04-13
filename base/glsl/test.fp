#include "global.inc"

in vs_output
{
  vec2 texcoord;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  vec4 color;
} fragment;

out vec4 result;

void main(void)
{
  result = texture2D(texture0, fragment.texcoord) * 1.2;
}