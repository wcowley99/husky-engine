#pragma once

#include "model.h"
#include "pipeline.h"
#include "vkb.h"

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

// abstract gpu functions
uint32_t gpu_upload_texture(MaterialInfo *mats);
GpuModel agpu_load_model(char *filename);

void gpu_unload_models();

VkBuffer gpu_mesh_index_buffer(uint32_t mesh);
uint32_t gpu_mesh_index_count(uint32_t mesh);
VkDeviceAddress gpu_mesh_vertex_address(uint32_t mesh);
