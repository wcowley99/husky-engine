#pragma once

#include "model/model.h"

#include "buffer.h"
#include "descriptors.h"
#include "pipeline.h"

#include "vkb.h"

#include "scene/camera.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>

typedef struct {
        Buffer vertex;
        Buffer index;
        VkDeviceAddress vertex_address;

        uint32_t num_indices;
} MeshBuffer;

uint32_t mesh_buffer_create(Mesh *mesh);
void mesh_buffer_destroy(MeshBuffer *buffer);

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

typedef enum {
        MATERIAL_PASS_COLOR,
        MATERIAL_PASS_TRANSPARENT,
        MATERIAL_PASS_OTHER,
} MaterialPass;

typedef struct {
        GraphicsPipeline *pipeline;
        VkDescriptorSet material_set;
        MaterialPass pass;
} Material;

typedef struct {
        uint32_t mesh;
        uint32_t diffuse_tex;

        Material *material;
} GpuMesh;

typedef struct {
        GpuMesh *meshes;
} GpuModel;

typedef struct {
        uint32_t mesh;
        uint32_t tex_index;

        Material *material;
        mat4 transform;
} RenderObject;

// TODO: GET RID OF THIS !!!
extern RenderObject *g_RenderObjects;

bool render_object_compatible(RenderObject a, RenderObject b);

RenderObject gpu_upload_model(Model *model);

uint32_t gpu_upload_texture(MaterialInfo *mats);

typedef struct {
        RenderObject object;

        uint32_t first_instance;
        uint32_t count;
} DrawBatch;

DrawBatch draw_batch_create(RenderObject obj, uint32_t first_instance);
void draw_batch_add(DrawBatch *batch, RenderObject obj, Instance *ssbo);

void draw_batch_draw(DrawBatch *batch);

// Vulkan Initialization Functions

bool init_descriptors();

bool create_vma_allocator();

void create_samplers();

// abstract gpu functions
GpuModel agpu_load_model(char *filename);

void agpu_begin_frame();
void agpu_end_frame();

void agpu_set_camera(Camera camera);

void agpu_draw_model(GpuModel model, mat4 transform);
