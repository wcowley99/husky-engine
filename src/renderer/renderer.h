#pragma once

#include "common/linalgebra.h"
#include "model/model.h"

#include "buffer.h"

#include "vkb.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stdint.h>

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

typedef struct {
        mat4 view;
        mat4 proj;
        mat4 viewproj;
} CameraData;

typedef struct {
        mat4 matrix;
        VkDeviceAddress vertex_address;
} MeshPushConstants;

typedef struct {
        Buffer vertex;
        Buffer index;
        VkDeviceAddress vertex_address;
} MeshBuffer;

bool mesh_buffer_create(Mesh *mesh, MeshBuffer *buffer);
void mesh_buffer_destroy(MeshBuffer *buffer);

///////////////////////////////////////
/// FrameResources
///////////////////////////////////////
typedef struct {
        VkCommandPool pool;
        VkCommandBuffer command;
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence render_fence;

        Buffer camera_uniform;

        VkDescriptorSet global_descriptors;
        VkDescriptorSet mat_descriptors;
} FrameResources;

bool frame_resources_create(FrameResources *f);

void frame_resources_destroy(FrameResources *f);

bool frame_resources_submit(FrameResources *f);

///////////////////////////////////////
/// Compute Pipeline
///////////////////////////////////////

typedef struct {
        VkDescriptorSetLayout *descriptors;
        uint32_t num_descriptors;

        const uint32_t *push_constant_sizes;
        uint32_t num_push_constant_sizes;

        const uint32_t *shader_source;
        uint32_t shader_source_size;
} ComputePipelineInfo;

typedef struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} ComputePipeline;

bool compute_pipeline_create(ComputePipelineInfo *info, ComputePipeline *p);

void compute_pipeline_destroy(ComputePipeline *p);

///////////////////////////////////////
/// Graphics Pipeline
///////////////////////////////////////

typedef struct {
        VkDescriptorSetLayout *descriptors;
        uint32_t num_descriptors;

        const VkPushConstantRange *push_constants;
        uint32_t num_push_constants;

        VkPrimitiveTopology topology;
        VkPolygonMode polygon_mode;
        VkCullModeFlagBits cull_mode;
        VkFrontFace front_face;
        VkFormat color_attachment_format;
        VkFormat depth_attachment_format;

        const uint32_t *vertex_shader;
        uint32_t vertex_shader_size;

        const uint32_t *fragment_shader;
        uint32_t fragment_shader_size;

        bool depth_testing;
        VkCompareOp depth_compare_op;
} GraphicsPipelineCreateInfo;

typedef struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} GraphicsPipeline;

bool graphics_pipeline_create(GraphicsPipelineCreateInfo *create_info, GraphicsPipeline *pipeline);

void graphics_pipeline_destroy(GraphicsPipeline *pipeline);

///////////////////////////////////////
/// Swapchain
///////////////////////////////////////

bool swapchain_create();
void swapchain_destroy();
void SwapchainRecreate();

bool swapchain_next_frame();

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

void RendererDraw();

void MoveCamera(vec3 delta);

bool init_textures();

// Vulkan Initialization Functions

bool init_descriptors();

bool create_vma_allocator();

bool create_instance(RendererCreateInfo *c);
bool create_debug_messenger();
bool create_surface();

bool is_gpu_suitable(VkPhysicalDevice gpu);
bool select_gpu();

bool create_device();

bool begin_command_buffer(VkCommandBuffer command);

bool create_shader_module(const uint32_t *bytes, size_t size, VkShaderModule *module);

bool create_descriptor_layout(VkDescriptorSetLayoutBinding *bindings, uint32_t count,
                              VkDescriptorSetLayout *layout);
