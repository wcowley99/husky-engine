#pragma once

#include <cglm/cglm.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct vertex {
        vec3 position;
        float uv_x;
        vec3 normal;
        float uv_y;
        vec4 color;
} vertex_t;

typedef struct mesh {
        vertex_t *vertices;
        uint32_t *indices;

        uint32_t material_index;
} mesh_t;

typedef struct material_info {
        vec3 ambient;
        vec3 specular;

        char *diffuse_tex;
        size_t diffuse_width;
        size_t diffuse_height;
} material_info_t;

typedef struct model {
        mesh_t *meshes;

        material_info_t *materials;
} model_t;

model_t load_model(char *filename);
void model_destroy(model_t m);
