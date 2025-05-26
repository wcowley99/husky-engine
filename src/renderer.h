#pragma once

#include <SDL3/SDL.h>
#include <cglm/cglm.h>

#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdint.h>

#define EXPECT(x)                                                                                  \
  do {                                                                                             \
    if (!(x)) {                                                                                    \
      printf("[%s:%d] EXPECT failed: %s\n", __FILE__, __LINE__, #x);                               \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

#define VK_EXPECT(x)                                                                               \
  do {                                                                                             \
    VkResult err = x;                                                                              \
    if (err != VK_SUCCESS) {                                                                       \
      printf("[%s:%d] VK_EXPECT failed: %s\n", __FILE__, __LINE__, string_VkResult(err));          \
      return false;                                                                                \
    }                                                                                              \
  } while (false)

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

///////////////////////////////////////
/// Immediate Command
///////////////////////////////////////

typedef struct {
  VkCommandPool pool;
  VkCommandBuffer command;
  VkFence fence;
} ImmediateCommand;

bool immediate_command_create(ImmediateCommand *command);

void immediate_command_destroy(ImmediateCommand *command);

void immediate_command_begin(ImmediateCommand *command);
void immediate_command_end(ImmediateCommand *command);

///////////////////////////////////////
/// Image
///////////////////////////////////////
typedef struct {
  VkImage image;
  VkImageView image_view;
  VkExtent3D extent;
  VkFormat format;
  VkImageLayout layout;
} Image;

bool image_create(VkImage vk_image, VkExtent2D extent, VkFormat format, VkImageLayout layout,
                  Image *image);

void image_destroy(Image *image);

void image_transition(Image *image, VkCommandBuffer command, VkImageLayout layout);

void image_blit(VkCommandBuffer command, Image *src, Image *dst);

typedef struct {
  VkExtent3D extent;
  VkFormat format;
  VkImageUsageFlags usage_flags;
  VmaMemoryUsage memory_usage;
  VkMemoryPropertyFlags memory_props;
} AllocatedImageCreateInfo;

typedef struct {
  Image image;
  VmaAllocation allocation;
} AllocatedImage;

bool allocated_image_create(AllocatedImageCreateInfo *info, AllocatedImage *image);

void allocated_image_destroy(AllocatedImage *image);

///////////////////////////////////////
/// FrameResources
///////////////////////////////////////
typedef struct {
  VkCommandPool pool;
  VkCommandBuffer command;
  VkSemaphore swapchain_semaphore;
  VkSemaphore render_semaphore;
  VkFence render_fence;
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

typedef struct {
  VkSwapchainKHR swapchain;
  VkFormat format;
  uint32_t image_count;
  uint32_t frame_index;
  Image *images;
  FrameResources *frames;
} Swapchain;

bool swapchain_create();

void swapchain_destroy();

bool swapchain_next_frame(FrameResources **frame, Image **image, uint32_t *image_index);

///////////////////////////////////////
/// Descriptor Allocator
///////////////////////////////////////

typedef struct {
  VkDescriptorPool pool;
} DescriptorAllocator;

bool descriptor_allocator_create(DescriptorAllocator *allocator);

void descriptor_allocator_destroy(DescriptorAllocator *allocator);

void descriptor_allocator_clear(DescriptorAllocator *allocator);

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDescriptorSetLayout *layouts,
                                   uint32_t count, VkDescriptorSet *sets);

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

// Vulkan Initialization Functions

void vk_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *info);

bool vma_allocator();

bool vk_create_instance(RendererCreateInfo *c);
bool vk_create_debug_messenger();
bool vk_create_surface();

bool vk_gpu_suitable(VkPhysicalDevice gpu);
bool vk_select_gpu();

bool vk_create_device();

bool vk_begin_command_buffer(VkCommandBuffer command);

bool vk_create_shader_module(VkShaderModule *module, const uint32_t *bytes, size_t size);

bool vk_descriptor_pool(VkDescriptorPool *pool, VkDescriptorPoolSize *sizes, uint32_t count);

bool vk_descriptor_layout(VkDescriptorSetLayout *layout, VkDescriptorSetLayoutBinding *bindings,
                          uint32_t count);
