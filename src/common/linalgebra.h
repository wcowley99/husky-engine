#pragma once

typedef union {
        struct {
                float x, y, z;
        };
        struct {
                float r, g, b;
        };
        float elements[3];
} vec3;

typedef union {
        struct {
                float x, y, z, w;
        };
        struct {
                float r, g, b, a;
        };
        float elements[4];
} vec4;

/**
 * Column-Major 4x4 Matrix
 *
 * [ vx ] [ vy ] [ vz ] [ vw ]
 *  ....   ....   ....   ....
 * [ m00] [ m10] [ m20] [ m30]
 * [ m01] [ m11] [ m21] [ m31]
 * [ m02] [ m12] [ m22] [ m32]
 * [ m03] [ m13] [ m23] [ m33]
 */
typedef union {
        struct {
                vec4 vx, vy, vz, vw;
        };
        struct {
                float m00, m01, m02, m03;
                float m10, m11, m12, m13;
                float m20, m21, m22, m23;
                float m30, m31, m32, m33;
        };
        float mm[4][4];
        float elements[16];
} mat4;

#define MAT4_IDENTITY                                                                              \
        (mat4) {                                                                                   \
                {                                                                                  \
                        {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},                        \
                            {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}                     \
                }                                                                                  \
        }

#define MATH_PI 3.14159265358979323846264

float to_radians(float degrees);
float to_degrees(float radians);

void vec3_print(vec3 v, const char *name);
void vec4_print(vec4 v, const char *name);
void mat4_print(mat4 v, const char *name);

vec3 vec3_add(vec3 a, vec3 b);
vec4 vec4_add(vec4 a, vec4 b);

vec3 vec3_sub(vec3 a, vec3 b);
vec4 vec4_sub(vec4 a, vec4 b);

vec3 vec3_normalize(vec3 vec);

vec3 vec3_cross(vec3 a, vec3 b);

float vec3_dot(vec3 a, vec3 b);

mat4 mat4_mult(mat4 a, mat4 b);
vec4 mat4_mul_vec(mat4 a, vec4 b);

mat4 mat4_look_at(vec3 position, vec3 target, vec3 up);
mat4 mat4_perspective(float fov, float aspect, float near, float far);
mat4 mat4_frustum(float left, float right, float top, float bottom, float near, float far);
