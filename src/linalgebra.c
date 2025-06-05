#include "linalgebra.h"

#include <math.h>
#include <stdio.h>

float to_radians(float degrees) { return degrees * MATH_PI / 180.0f; }

float to_degrees(float radians) { return radians * 180.0f / MATH_PI; }

void vec3_print(vec3 v, const char *name) {
        printf("[%s] = {%.02f, %.02f, %.02f}\n", name, v.x, v.y, v.z);
}

void vec4_print(vec4 v, const char *name) {
        printf("[%s] = {%.02f, %.02f, %.02f, %.02f}\n", name, v.x, v.y, v.z, v.w);
}

void mat4_print(mat4 v, const char *name) {
        printf("[%s]:\n", name);
        printf("{%.02f, %.02f, %.02f, %.02f}\n", v.m01, v.m10, v.m20, v.m30);
        printf("{%.02f, %.02f, %.02f, %.02f}\n", v.m01, v.m11, v.m21, v.m31);
        printf("{%.02f, %.02f, %.02f, %.02f}\n", v.m02, v.m12, v.m22, v.m32);
        printf("{%.02f, %.02f, %.02f, %.02f}\n", v.m03, v.m13, v.m23, v.m33);
}

vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }

vec4 vec4_add(vec4 a, vec4 b) { return (vec4){a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }

vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
vec4 vec4_sub(vec4 a, vec4 b) { return (vec4){a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

float vec3_magnitude(vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

vec3 vec3_normalize(vec3 v) {
        float mag = vec3_magnitude(v);
        return (vec3){v.x / mag, v.y / mag, v.z / mag};
}

vec3 vec3_cross(vec3 a, vec3 b) {
        return (vec3){
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
}

float vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

mat4 mat4_mult(mat4 a, mat4 b) {
        return (mat4){
            a.m00 * b.m00 + a.m10 * b.m01 + a.m20 * b.m02 + a.m30 * b.m03,
            a.m01 * b.m00 + a.m11 * b.m01 + a.m21 * b.m02 + a.m31 * b.m03,
            a.m02 * b.m00 + a.m12 * b.m01 + a.m22 * b.m02 + a.m32 * b.m03,
            a.m03 * b.m00 + a.m13 * b.m01 + a.m23 * b.m02 + a.m33 * b.m03,

            a.m00 * b.m10 + a.m10 * b.m11 + a.m20 * b.m12 + a.m30 * b.m13,
            a.m01 * b.m10 + a.m11 * b.m11 + a.m21 * b.m12 + a.m31 * b.m13,
            a.m02 * b.m10 + a.m12 * b.m11 + a.m22 * b.m12 + a.m32 * b.m13,
            a.m03 * b.m10 + a.m13 * b.m11 + a.m23 * b.m12 + a.m33 * b.m13,

            a.m00 * b.m20 + a.m10 * b.m21 + a.m20 * b.m22 + a.m30 * b.m23,
            a.m01 * b.m20 + a.m11 * b.m21 + a.m21 * b.m22 + a.m31 * b.m23,
            a.m02 * b.m20 + a.m12 * b.m21 + a.m22 * b.m22 + a.m32 * b.m23,
            a.m03 * b.m20 + a.m13 * b.m21 + a.m23 * b.m22 + a.m33 * b.m23,

            a.m00 * b.m30 + a.m10 * b.m31 + a.m20 * b.m32 + a.m30 * b.m33,
            a.m01 * b.m30 + a.m11 * b.m31 + a.m21 * b.m32 + a.m31 * b.m33,
            a.m02 * b.m30 + a.m12 * b.m31 + a.m22 * b.m32 + a.m32 * b.m33,
            a.m03 * b.m30 + a.m13 * b.m31 + a.m23 * b.m32 + a.m33 * b.m33,
        };
}

vec4 mat4_mul_vec(mat4 a, vec4 b) {
        return (vec4){
            a.m00 * b.x + a.m10 * b.y + a.m20 * b.z + a.m30 * b.w,
            a.m01 * b.x + a.m11 * b.y + a.m21 * b.z + a.m31 * b.w,
            a.m02 * b.x + a.m12 * b.y + a.m22 * b.z + a.m32 * b.w,
            a.m03 * b.x + a.m13 * b.y + a.m23 * b.z + a.m33 * b.w,
        };
}

mat4 mat4_look_at(vec3 position, vec3 target, vec3 up) {
        vec3 forward = vec3_normalize(vec3_sub(position, target));
        vec3 right = vec3_normalize(vec3_cross(up, forward));
        vec3 yaxis = vec3_cross(forward, right);

        return (mat4){{
            {right.x, yaxis.x, forward.x, 0.0f},
            {right.y, yaxis.y, forward.y, 0.0f},
            {right.z, yaxis.z, forward.z, 0.0f},
            {-vec3_dot(right, position), -vec3_dot(yaxis, position), -vec3_dot(forward, position),
             1.0f},
        }};
}

mat4 mat4_perspective(float fov, float aspect, float near, float far) {
        float top = near * tanf(fov / 2.0f);
        float right = top * aspect;

        top *= -1; // negate top so +y = up

        return mat4_frustum(-right, right, top, -top, near, far);
}

mat4 mat4_frustum(float left, float right, float top, float bottom, float near, float far) {
        return (mat4){{
            {near / right, 0, 0, 0},
            {0, near / top, 0, 0},
            {0, 0, -(far + near) / (far - near), -1},
            {0, 0, -2 * far * near / (far - near), 0},
        }};
}
