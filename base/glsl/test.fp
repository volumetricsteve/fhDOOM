#version 330
#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0)  uniform mat4 rpModelMatrix;
layout(location = 1)  uniform mat4 rpViewMatrix;
layout(location = 2)  uniform mat4 rpModelViewMatrix;
layout(location = 3)  uniform mat4 rpProjectionMatrix;

layout(location = 4)  uniform vec4 rpLocalLightOrigin;
layout(location = 5)  uniform mat4 rpLocalViewOrigin;
layout(location = 6)  uniform mat4 rpLightProjection;
layout(location = 7)  uniform mat4 rpBumpMatrix;
layout(location = 8)  uniform mat4 rpDiffuseMatrix;
layout(location = 9)  uniform mat4 rpSpecularMatrix;
layout(location = 10) uniform mat4 rpColorModulate;
layout(location = 11) uniform mat4 rpColorAdd;
layout(location = 12) uniform mat4 rpDiffuseColor;
layout(location = 13) uniform mat4 rpSpecularColor;

layout(location = 14) uniform vec4 shaderParm0;
layout(location = 15) uniform vec4 shaderParm1;
layout(location = 16) uniform vec4 shaderParm2;
layout(location = 17) uniform vec4 shaderParm3;

layout(location = 18) uniform sampler2D texture0;
layout(location = 19) uniform sampler2D texture1;
layout(location = 20) uniform sampler2D texture2;
layout(location = 21) uniform sampler2D texture3;
layout(location = 22) uniform sampler2D texture4;
layout(location = 23) uniform sampler2D texture5;
layout(location = 24) uniform sampler2D texture6;
layout(location = 25) uniform sampler2D texture7;

// ----------------- end of uniforms  ----------------------

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