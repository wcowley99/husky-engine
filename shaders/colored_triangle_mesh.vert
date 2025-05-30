#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform CameraData {
        mat4 view;
        mat4 proj;
        mat4 viewproj;
}
camera_data;

struct Vertex {
        vec3 position;
        float uv_x;
        vec3 normal;
        float uv_y;
        vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer { Vertex vertices[]; };

// push constants block
layout(push_constant) uniform constants {
        mat4 model;
        VertexBuffer vertexBuffer;
}
PushConstants;

void main() {
        // load vertex data from device adress
        Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

        // output data
        gl_Position = (camera_data.viewproj * PushConstants.model) * vec4(v.position, 1.0f);
        outColor = v.color.xyz;
        outUV.x = v.uv_x;
        outUV.y = v.uv_y;
}