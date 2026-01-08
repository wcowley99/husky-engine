#version 450

// shader input
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 outNormal;
layout(location = 2) in vec2 inUV;

// output write
layout(location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform sampler2D mesh_texture;

void main() {
  outFragColor = texture(mesh_texture, inUV);
  // outFragColor = vec4(inUV, 0.0, 1.0f);
}
