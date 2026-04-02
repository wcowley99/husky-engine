#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "input_structures.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in int inTexIndex;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 2) uniform sampler2D diffuse_textures[];

void main() {
  float light = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1);

  vec3 color = inColor * texture(diffuse_textures[nonuniformEXT(inTexIndex)], inUV).xyz;
  vec3 ambient = color * sceneData.ambientColor.xyz;

  // fragColor = vec4(color * light * sceneData.sunlightColor.w + ambient, 1.0f);
  fragColor = vec4(ambient * light, 1.0f);
  // fragColor = texture(diffuse_textures[nonuniformEXT(inTexIndex)], inUV);
}
