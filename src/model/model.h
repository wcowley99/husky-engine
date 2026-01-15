#pragma once

#include "common/linalgebra.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
        vec3 position;
        float uv_x;
        vec3 normal;
        float uv_y;
        vec4 color;
} Vertex;

typedef struct {
        Vertex *vertices;
        uint32_t *indices;
} Mesh;

void MeshFree(Mesh *mesh);

typedef struct {
        float u;
        float v;
} TextureCoord;

typedef struct {
        char *name;

        vec3 ambient;
        vec3 specular;

        char *diffuse_tex;
        size_t diffuse_width;
        size_t diffuse_height;
} MaterialInfo;

void material_destroy(MaterialInfo *mat);

typedef struct {
        Mesh *meshes;
        size_t num_meshes;

        MaterialInfo *materials;
} Model;

Model load_obj(const char *filename);

Model load_model(const char *filename);

void model_destroy(Model m);
