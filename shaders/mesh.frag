#version 450

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) flat in int inTexIndex;

layout(location = 0) out vec4 fragColor;

void main() {
  // float light = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
  //
  // vec3 color = inColor * texture(colorTex, inUV).xyz;
  // vec3 ambient = color * sceneData.ambientColor.xyz;

  // fragColor = vec4(color * light * sceneData.sunlightColor.w + ambient, 1.0f);
  fragColor = texture(colorTex, inUV);
}
