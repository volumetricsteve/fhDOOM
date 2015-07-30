#include "global.inc"

in vs_output
{
  vec4 color;
} frag;

out vec4 result;

void main(void)
{  
  result = frag.color;
}