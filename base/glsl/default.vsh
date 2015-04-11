#version 330

uniform mat4 rpModelView;
uniform mat4 rpProjection;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec2 vertex_texcoord;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec4 vertex_color;
layout(location = 4) in vec3 vertex_binormal;
layout(location = 5) in vec3 vertex_tangent;

out vs_output
{
  vec2 texcoord;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  vec4 color;
} result;

void main(void)
{
  gl_Position = rpProjection * rpModelView * vec4(vertex_position, 1.0);

  result.texcoord = vertex_texcoord;
  result.normal = vertex_normal;
  result.binormal = vertex_binormal;
  result.tangent = vertex_tangent;
  result.color = vertex_color;
}