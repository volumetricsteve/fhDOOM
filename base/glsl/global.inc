#version 330
#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0)  uniform mat4 rpModelMatrix;
layout(location = 1)  uniform mat4 rpViewMatrix;
layout(location = 2)  uniform mat4 rpModelViewMatrix;
layout(location = 3)  uniform mat4 rpProjectionMatrix;

layout(location = 4)  uniform vec4 rpLocalLightOrigin;
layout(location = 5)  uniform vec4 rpLocalViewOrigin;
layout(location = 6)  uniform vec4 rpLightProjectionS;
layout(location = 7)  uniform vec4 rpLightProjectionT;
layout(location = 8)  uniform vec4 rpLightProjectionQ;
layout(location = 9)  uniform vec4 rpLightFallOff;

layout(location = 10) uniform vec4 rpBumpMatrixS;
layout(location = 11) uniform vec4 rpBumpMatrixT;
layout(location = 12) uniform vec4 rpDiffuseMatrixS;
layout(location = 13) uniform vec4 rpDiffuseMatrixT;
layout(location = 14) uniform vec4 rpSpecularMatrixS;
layout(location = 15) uniform vec4 rpSpecularMatrixT;

layout(location = 16) uniform vec4 rpColorModulate;
layout(location = 17) uniform vec4 rpColorAdd;
layout(location = 18) uniform vec4 rpDiffuseColor;
layout(location = 19) uniform vec4 rpSpecularColor;

layout(location = 20) uniform vec4 shaderParm0;
layout(location = 21) uniform vec4 shaderParm1;
layout(location = 22) uniform vec4 shaderParm2;
layout(location = 23) uniform vec4 shaderParm3;

layout(location = 24) uniform sampler2D texture0;
layout(location = 25) uniform sampler2D texture1;
layout(location = 26) uniform sampler2D texture2;
layout(location = 27) uniform sampler2D texture3;
layout(location = 28) uniform sampler2D texture4;
layout(location = 29) uniform sampler2D texture5;
layout(location = 30) uniform sampler2D texture6;
layout(location = 31) uniform sampler2D texture7;

layout(location = 40) uniform samplerCube cubemap0;
layout(location = 41) uniform samplerCube cubemap1;
layout(location = 42) uniform samplerCube cubemap2;
layout(location = 43) uniform samplerCube cubemap3;
layout(location = 44) uniform samplerCube cubemap4;
layout(location = 45) uniform samplerCube cubemap5;
layout(location = 46) uniform samplerCube cubemap6;
layout(location = 47) uniform samplerCube cubemap7;

layout(location = 50) uniform mat4 textureMatrix0;
layout(location = 51) uniform mat4 textureMatrix1;
layout(location = 52) uniform mat4 textureMatrix2;
layout(location = 53) uniform mat4 textureMatrix3;

layout(location = 100) uniform int rpAlphaTestEnabled;
layout(location = 101) uniform float rpAlphaTestThreshold;
layout(location = 102) uniform vec4 rpCurrentRenderSize;
