#include "renderer/gpu_model.h"

#include "renderer/image.h"
#include "renderer/renderer.h"
#include "renderer/sampler.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "common/array.h"

typedef struct {
        Buffer vertex;
        Buffer index;
        VkDeviceAddress vertex_address;

        uint32_t num_indices;
} MeshBuffer;

static MeshBuffer *g_MeshBuffers;
static AllocatedImage *g_Textures;

static uint32_t mesh_buffer_create(Mesh *mesh) {
        static int init = 0;
        if (!init) {
                g_MeshBuffers = array(MeshBuffer);
                g_Textures = array(AllocatedImage);
                init = 1;
        }

        const size_t vertex_buffer_size = sizeof(Vertex) * array_length(mesh->vertices);
        const size_t index_buffer_size = sizeof(uint32_t) * array_length(mesh->indices);

        uint32_t index = array_length(g_MeshBuffers);
        array_append(g_MeshBuffers, (MeshBuffer){0});
        MeshBuffer *buffer = &g_MeshBuffers[index];
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

        Buffer staging_buffer;
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

static void mesh_buffer_destroy(MeshBuffer *buffer) {
        buffer_destroy(&buffer->vertex);
        buffer_destroy(&buffer->index);
}

uint32_t gpu_upload_texture(MaterialInfo *mats) {
        AllocatedImageCreateInfo create_info = {
            .extent = (VkExtent3D){mats->diffuse_width, mats->diffuse_height, 1},
            .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,

            .data = (uint32_t *)mats->diffuse_tex,
        };

        uint32_t index = array_length(g_Textures);
        array_append(g_Textures, (AllocatedImage){0});
        AllocatedImage *image = &g_Textures[index];
        allocated_image_create(&create_info, image);

        swapchain_descriptors_write_texture(&g_Textures[index].image, index, linear_sampler());

        return index;
}

GpuModel renderer_load_model(char *filename) {
        Model m = load_model(filename);
        GpuModel r;
        r.meshes = array(GpuMesh);

        DEBUG("#meshes = %zu, #materials = %zu", array_length(m.meshes), array_length(m.materials));

        for (int i = 0; i < array_length(m.meshes); i++) {
                Mesh *cpu_mesh = &m.meshes[i];
                MaterialInfo *material = &m.materials[cpu_mesh->material_index];
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
        for (int i = 0; i < array_length(g_MeshBuffers); i += 1) {
                mesh_buffer_destroy(&g_MeshBuffers[i]);
        }
        array_free(g_MeshBuffers);

        for (int i = 0; i < array_length(g_Textures); i += 1) {
                allocated_image_destroy(&g_Textures[i], vk_context_device());
        }
        array_free(g_Textures);
}

VkBuffer gpu_mesh_index_buffer(uint32_t mesh) { return g_MeshBuffers[mesh].index.buffer; }
uint32_t gpu_mesh_index_count(uint32_t mesh) { return g_MeshBuffers[mesh].num_indices; }

VkDeviceAddress gpu_mesh_vertex_address(uint32_t mesh) {
        return g_MeshBuffers[mesh].vertex_address;
}
