#version 450

// shader input
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 outNormal;
layout(location = 2) in vec2 inUV;

// output write
layout(location = 0) out vec4 outFragColor;

void main() {
    outFragColor = vec4(outNormal, 1.0f);
}
