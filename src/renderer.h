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
/// Image
///////////////////////////////////////
typedef struct {
  VkImage image;
  VkImageView image_view;
  VkExtent3D extent;
  VkFormat format;
  VkImageLayout layout;
} Image;

bool image_create(VkDevice device, VkImage vk_image, VkExtent2D extent, VkFormat format,
                  VkImageLayout layout, Image *image);

void image_destroy(Image *image, VkDevice device);

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

bool allocated_image_create(VkDevice device, VmaAllocator allocator, AllocatedImageCreateInfo *info,
                            AllocatedImage *image);

void allocated_image_destroy(AllocatedImage *image, VkDevice device, VmaAllocator allocator);

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

bool frame_resources_create(VkDevice device, uint32_t queue_family_index, FrameResources *f);

void frame_resources_destroy(FrameResources *f, VkDevice device);

bool frame_resources_submit(FrameResources *f, VkQueue graphics_queue);

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

bool compute_pipeline_create(ComputePipeline *p, ComputePipelineInfo *info, VkDevice device);

void compute_pipeline_destroy(ComputePipeline *p, VkDevice device);

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

bool swapchain_create(VkDevice device, VkPhysicalDevice gpu, VkSurfaceKHR surface,
                      uint32_t queue_family_index, Swapchain *swapchain);

void swapchain_destroy(Swapchain *sc, VkDevice device);

bool swapchain_next_frame(Swapchain *sc, VkDevice device, FrameResources **frame, Image **image,
                          uint32_t *image_index);

///////////////////////////////////////
/// Descriptor Allocator
///////////////////////////////////////

typedef struct {
  VkDescriptorPool pool;
} DescriptorAllocator;

bool descriptor_allocator_create(DescriptorAllocator *allocator, VkDevice device);

void descriptor_allocator_destroy(DescriptorAllocator *allocator, VkDevice device);

void descriptor_allocator_clear(DescriptorAllocator *allocator, VkDevice device);

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDevice device,
                                   VkDescriptorSetLayout *layouts, uint32_t count,
                                   VkDescriptorSet *sets);

///////////////////////////////////////
/// Renderer
///////////////////////////////////////

typedef struct {
  uint32_t width;
  uint32_t height;
  const char *title;
  bool debug;
} RendererCreateInfo;

typedef struct {
  SDL_Window *window;

  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  VkPhysicalDevice gpu;
  uint32_t queue_family_index;
  VkDevice device;
  VkQueue graphics_queue;

  VmaAllocator allocator;

  Swapchain swapchain;
  AllocatedImage draw_image;

  DescriptorAllocator global_descriptor_allocator;
  VkDescriptorSetLayout draw_image_descriptor_layout;
  VkDescriptorSet draw_image_descriptors;

  ComputePipeline gradient_pipeline;
} Renderer;

// Renderer Core Functions

bool renderer_init(Renderer *r, RendererCreateInfo *c);
void renderer_shutdown(Renderer *r);

void renderer_draw(Renderer *r);

// Vulkan Initialization Functions

void vk_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *info);

bool vma_allocator(VmaAllocator *allocator, VkInstance instance, VkPhysicalDevice gpu,
                   VkDevice device);

bool vk_create_instance(VkInstance *instance, RendererCreateInfo *c);
bool vk_create_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT *debug_messenger);
bool vk_create_surface(VkInstance instance, SDL_Window *window, VkSurfaceKHR *surface);

bool vk_gpu_suitable(VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *queue_family_index);
bool vk_select_gpu(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice *gpu,
                   uint32_t *queue_family_index);

bool vk_create_device(VkInstance instance, VkPhysicalDevice gpu, uint32_t queue_family_index,
                      VkDevice *device, VkQueue *graphics_queue);

bool vk_begin_command_buffer(VkCommandBuffer command);

bool vk_create_shader_module(VkShaderModule *module, const uint32_t *bytes, size_t size,
                             VkDevice device);

bool vk_descriptor_pool(VkDescriptorPool *pool, VkDevice device, VkDescriptorPoolSize *sizes,
                        uint32_t count);

bool vk_descriptor_layout(VkDescriptorSetLayout *layout, VkDevice device,
                          VkDescriptorSetLayoutBinding *bindings, uint32_t count);
