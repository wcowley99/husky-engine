#pragma once

#include <cglm/cglm.h>

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

        uint32_t material_index;
} Mesh;

void MeshFree(Mesh *mesh);

typedef struct {
        float u;
        float v;
} TextureCoord;

typedef struct {
        vec3 ambient;
        vec3 specular;

        char *diffuse_tex;
        size_t diffuse_width;
        size_t diffuse_height;
} MaterialInfo;

void material_info_destroy(MaterialInfo *mat);

typedef struct {
        Mesh *meshes;

        MaterialInfo *materials;
} Model;

Model load_model(char *filename);

void model_destroy(Model m);
