#pragma once

#include "common/linalgebra.h"

#include <stdbool.h>
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

bool LoadObjFromFile(Mesh *mesh, const char *filename);
bool LoadFromFile(Mesh *mesh, const char *filename);
