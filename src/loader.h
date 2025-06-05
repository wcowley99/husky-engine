#pragma once

#include "linalgebra.h"

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
        uint32_t num_vertices;
        uint32_t num_indices;
} Mesh;

Mesh *LoadObjFromFile(const char *filename);

Mesh *LoadFromFile(const char *filename);
