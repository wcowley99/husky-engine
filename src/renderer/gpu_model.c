#include "renderer/gpu_model.h"

#include "renderer/command.h"
#include "renderer/image.h"
#include "renderer/renderer.h"
#include "renderer/sampler.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "common/array.h"

typedef struct mesh_buffer {
        buffer_t vertex;
        buffer_t index;
        VkDeviceAddress vertex_address;

        uint32_t num_indices;
} mesh_buffer_t;

static mesh_buffer_t *g_mesh_buffers;
static AllocatedImage *g_textures;

static uint32_t mesh_buffer_create(mesh_t *mesh) {
        static int init = 0;
        if (!init) {
                g_mesh_buffers = array(mesh_buffer_t);
                g_textures = array(AllocatedImage);
                init = 1;
        }

        const size_t vertex_buffer_size = sizeof(vertex_t) * array_length(mesh->vertices);
        const size_t index_buffer_size = sizeof(uint32_t) * array_length(mesh->indices);

        uint32_t index = array_length(g_mesh_buffers);
        array_append(g_mesh_buffers, (mesh_buffer_t){0});
        mesh_buffer_t *buffer = &g_mesh_buffers[index];
        buffer->num_indices = array_length(mesh->indices);

        buffer_create(vertex_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->vertex);
        buffer_create(index_buffer_size,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->index);

        VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer->vertex.buffer,
        };
        buffer->vertex_address = vkGetBufferDeviceAddress(vk_context_device(), &address_info);

        buffer_t staging_buffer;
        buffer_create(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, &staging_buffer);
        vmaCopyMemoryToAllocation(vk_memory_allocator(), mesh->vertices, staging_buffer.allocation,
                                  0, vertex_buffer_size);
        vmaCopyMemoryToAllocation(vk_memory_allocator(), mesh->indices, staging_buffer.allocation,
                                  vertex_buffer_size, index_buffer_size);

        VkCommandBuffer cmd = immediate_command_begin();

        VkBufferCopy vertex_copy = {.size = vertex_buffer_size};
        VkBufferCopy index_copy = {.size = index_buffer_size, .srcOffset = vertex_buffer_size};

        vkCmdCopyBuffer(cmd, staging_buffer.buffer, buffer->vertex.buffer, 1, &vertex_copy);
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, buffer->index.buffer, 1, &index_copy);

        immediate_command_end();

        buffer_destroy(&staging_buffer);
        return index;
}

static void mesh_buffer_destroy(mesh_buffer_t *buffer) {
        buffer_destroy(&buffer->vertex);
        buffer_destroy(&buffer->index);
}

uint32_t gpu_upload_texture(material_info_t *mats) {
        AllocatedImageCreateInfo create_info = {
            .extent = (VkExtent3D){mats->diffuse_width, mats->diffuse_height, 1},
            .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,

            .data = (uint32_t *)mats->diffuse_tex,
        };

        uint32_t index = array_length(g_textures);
        array_append(g_textures, (AllocatedImage){0});
        AllocatedImage *image = &g_textures[index];
        allocated_image_create(&create_info, image);

        swapchain_descriptors_write_texture(&g_textures[index].image, index, linear_sampler());

        return index;
}

GpuModel renderer_load_model(char *filename) {
        model_t m = load_model(filename);
        GpuModel r;
        r.meshes = array(GpuMesh);

        DEBUG("#meshes = %zu, #materials = %zu", array_length(m.meshes), array_length(m.materials));

        for (int i = 0; i < array_length(m.meshes); i++) {
                mesh_t *cpu_mesh = &m.meshes[i];
                material_info_t *material = &m.materials[cpu_mesh->material_index];
                GpuMesh mesh = {
                    .mesh = mesh_buffer_create(cpu_mesh),
                    .material.diffuse_tex = gpu_upload_texture(material),
                };

                array_append(r.meshes, mesh);
        }

        model_destroy(m);

        return r;
}

void gpu_unload_models() {
        for (int i = 0; i < array_length(g_mesh_buffers); i += 1) {
                mesh_buffer_destroy(&g_mesh_buffers[i]);
        }
        array_free(g_mesh_buffers);

        for (int i = 0; i < array_length(g_textures); i += 1) {
                allocated_image_destroy(&g_textures[i], vk_context_device());
        }
        array_free(g_textures);
}

VkBuffer gpu_mesh_index_buffer(uint32_t mesh) { return g_mesh_buffers[mesh].index.buffer; }
uint32_t gpu_mesh_index_count(uint32_t mesh) { return g_mesh_buffers[mesh].num_indices; }

VkDeviceAddress gpu_mesh_vertex_address(uint32_t mesh) {
        return g_mesh_buffers[mesh].vertex_address;
}
