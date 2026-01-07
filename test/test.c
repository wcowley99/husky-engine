#include "src/common/linalgebra.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

uint32_t g_TestsPassed = 0;
uint32_t g_AssertsPassed = 0;

#define RUN_SUITE(name, x)                                                                         \
        do {                                                                                       \
                printf("Running Test Suite \"%s\" --------------------\n", name);                  \
                if (!x()) {                                                                        \
                        printf("TEST SUITE FAILED\n");                                             \
                        print_test_results();                                                      \
                        return 1;                                                                  \
                }                                                                                  \
        } while (false)

#define RUN_TEST(x)                                                                                \
        do {                                                                                       \
                bool result = x();                                                                 \
                printf(#x " .......... %s\n", result ? "true" : "false");                          \
                if (!result) {                                                                     \
                        return false;                                                              \
                } else {                                                                           \
                        g_TestsPassed += 1;                                                        \
                }                                                                                  \
        } while (false)

#define ASSERT(x)                                                                                  \
        do {                                                                                       \
                if (!x) {                                                                          \
                        printf("[%s:%d] - ASSERT failed: %s\n", __FILE__, __LINE__, #x);           \
                        return false;                                                              \
                } else {                                                                           \
                        g_AssertsPassed += 1;                                                      \
                }                                                                                  \
        } while (false)

void print_test_results() {
        printf("Tests Passed: %d ---- Assertions Passed: %d\n", g_TestsPassed, g_AssertsPassed);
}

bool float_fuzz_eq(float a, float b) {
        float diff = a - b;
        diff = diff < 0 ? -diff : diff;
        return (diff <= 0.00001f);
}

bool vec3_fuzz_eq(vec3 a, vec3 b) {
        return float_fuzz_eq(a.x, b.x) && float_fuzz_eq(a.y, b.y) && float_fuzz_eq(a.z, b.z);
}

bool vec4_fuzz_eq(vec4 a, vec4 b) {
        return float_fuzz_eq(a.x, b.x) && float_fuzz_eq(a.y, b.y) && float_fuzz_eq(a.z, b.z) &&
               float_fuzz_eq(a.w, b.w);
}

bool mat4_fuzz_eq(mat4 a, mat4 b) {
        return vec4_fuzz_eq(a.vx, b.vx) && vec4_fuzz_eq(a.vy, b.vy) && vec4_fuzz_eq(a.vz, b.vz) &&
               vec4_fuzz_eq(a.vw, b.vw);
}

bool test_vec3_add() {
        vec3 a = {1, 2, 3};
        ASSERT(vec3_fuzz_eq(vec3_add(a, a), (vec3){2, 4, 6}));

        vec3 b = {2, 4, 5};
        vec3 c = {-1, -2, -3};
        ASSERT(vec3_fuzz_eq(vec3_add(b, c), (vec3){1, 2, 2}));
        ASSERT(vec3_fuzz_eq(vec3_add(b, (vec3){-1, -2, -2}), a));

        return true;
}

bool test_mat4_mul_vec() {
        vec4 a = {1, 2, 3, 4};
        ASSERT(vec4_fuzz_eq(mat4_mul_vec(MAT4_IDENTITY, a), a));

        mat4 scale = MAT4_IDENTITY;
        scale.mm[0][0] = 2;
        scale.mm[1][1] = 0.5f;
        scale.mm[2][2] = 5.0f;
        ASSERT(vec4_fuzz_eq(mat4_mul_vec(scale, a), (vec4){2, 1, 15, 4}));

        scale.mm[3][0] = 1;
        ASSERT(vec4_fuzz_eq(mat4_mul_vec(scale, a), (vec4){6, 1, 15, 4}));

        return true;
}

bool test_mat4_mult() {
        mat4 a = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3};
        mat4 b = {2, 7, 1, 8, 2, 8, 1, 8, 2, 8, 4, 5, 9, 0, 4, 5};
        mat4 c = {118, 124, 99, 76, 123, 133, 101, 82, 111, 121, 89, 97, 92, 56, 101, 56};

        ASSERT(mat4_fuzz_eq(mat4_mult(a, b), c));
        ASSERT(mat4_fuzz_eq(mat4_mult(a, MAT4_IDENTITY), a));

        return true;
}

bool linalgebra_suite() {
        RUN_TEST(test_vec3_add);
        RUN_TEST(test_mat4_mult);
        RUN_TEST(test_mat4_mul_vec);
        return true;
}

int main(int argc, char **argv) {
        bool ok = true;

        RUN_SUITE("Linear Algebra", linalgebra_suite);

        print_test_results();

        return ok ^ true;
}
