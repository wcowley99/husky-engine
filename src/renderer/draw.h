#pragma once

#include "gpu_model.h"
#include "vkb.h"

typedef struct {
        uint32_t mesh;
        material_t material;
        mat4 transform;

        bool recorded;
} RenderObject;

void renderable_record(GpuModel model, mat4 transform);

typedef struct {
        uint32_t first_instance;
        uint32_t count;
        uint32_t mesh;
} DrawBatch;

void draw_buffers_init();
void draw_buffers_shutdown();
void draw_buffers_clear();

DrawBatch draw_batch_create(uint32_t mesh, uint32_t first_instance);
void draw_batch_add(DrawBatch *batch, RenderObject obj, Instance *ssbo);

void draw_batches_upload();
void draw_batches_record();
