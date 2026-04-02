#pragma once

#include "model.h"

#include "buffer.h"
#include "descriptors.h"
#include "pipeline.h"

#include "vkb.h"

#include "scene/camera.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>

///////////////////////////////////////
/// Renderer
///////////////////////////////////////

typedef struct {
        uint32_t width;
        uint32_t height;
        const char *title;
        bool debug;
} RendererCreateInfo;

// Renderer Core Functions

bool RendererInit(RendererCreateInfo *c);
void RendererShutdown();

void renderer_draw();

// Vulkan Initialization Functions

bool init_descriptors();

bool create_vma_allocator();

void agpu_begin_frame();
void agpu_end_frame();

void agpu_set_camera(Camera camera);
