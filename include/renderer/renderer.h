#pragma once

#include <cglm/cglm.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct renderer_config {
        uint32_t width;
        uint32_t height;
        const char *title;
        bool debug;
} renderer_config_t;

bool renderer_init(renderer_config_t *config);
void renderer_shutdown();

void renderer_begin_frame();
void renderer_end_frame();

void renderer_draw();

typedef struct material {
        uint32_t diffuse_tex;
} material_t;

typedef struct {
        uint32_t mesh;

        material_t material;
} GpuMesh;

typedef struct {
        GpuMesh *meshes;
} GpuModel;

GpuModel renderer_load_model(char *filename);

void renderable_record(GpuModel model, mat4 transform);
