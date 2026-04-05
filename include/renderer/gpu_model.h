#pragma once

#include "model.h"
#include "vkb.h"

// abstract gpu functions
uint32_t gpu_upload_texture(material_info_t *mats);

void gpu_unload_models();

VkBuffer gpu_mesh_index_buffer(uint32_t mesh);
uint32_t gpu_mesh_index_count(uint32_t mesh);
VkDeviceAddress gpu_mesh_vertex_address(uint32_t mesh);
