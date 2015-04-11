#version 330

uniform vec4 shaderParm0;
uniform vec4 shaderParm1;
uniform vec4 shaderParm2;
uniform vec4 shaderParm3;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture4;
uniform sampler2D texture5;
uniform sampler2D texture6;
uniform sampler2D texture7;

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
  result = texture2D(texture0, fragment.texcoord);
}