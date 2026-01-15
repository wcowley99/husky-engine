#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) flat out int outTexIndex;

struct Vertex {
  vec3 position;
  float uv_x;
  vec3 normal;
  float uv_y;
  vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
  Vertex vertices[];
};

struct Instance {
  mat4 model;
  VertexBuffer vertex_buffer;
  int tex_index;
  int padding;
};

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
  Instance instances[];
} instance_buffer;

void main() {
  Instance i = instance_buffer.instances[gl_InstanceIndex];
  Vertex v = i.vertex_buffer.vertices[gl_VertexIndex];

  gl_Position = sceneData.viewproj * i.model * vec4(v.position, 1.0f);
  outColor = v.color.xyz;
  outNormal = v.normal;
  outUV.x = v.uv_x;
  outUV.y = v.uv_y;
  outTexIndex = i.tex_index;
}
