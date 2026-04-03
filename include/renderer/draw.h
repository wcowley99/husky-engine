#pragma once

#include "gpu_model.h"
#include "vkb.h"

typedef struct {
        uint32_t first_instance;
        uint32_t count;
        uint32_t mesh;
} DrawBatch;

void draw_buffers_init();
void draw_buffers_shutdown();
void draw_buffers_clear();

void draw_batches_upload();
void draw_batches_record();
