#pragma once

#include <SDL3/SDL.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <assert.h>
#include <stdint.h>

#define EXPECT(x)                                                              \
  do {                                                                         \
    if (!(x)) {                                                                \
      printf("[%s:%d] EXPECT failed: %s\n", __FILE__, __LINE__, #x);           \
      return false;                                                            \
    }                                                                          \
  } while (false)

#define VK_EXPECT(x)                                                           \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err != VK_SUCCESS) {                                                   \
      printf("[%s:%d] VK_EXPECT failed: %s\n", __FILE__, __LINE__,             \
             string_VkResult(err));                                            \
      return false;                                                            \
    }                                                                          \
  } while (false)

VKAPI_ATTR VkBool32 VKAPI_CALL vk_validation_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

typedef struct {
  uint32_t width;
  uint32_t height;
  const char *title;
  bool debug;
} RendererCreateInfo;

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

bool image_create(VkDevice device, VkImage vk_image, VkExtent2D extent,
                  VkFormat format, VkImageLayout layout, Image *image);

void image_destroy(Image *image, VkDevice device);

void image_transition(Image *image, VkCommandBuffer command,
                      VkImageLayout layout);

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

bool frame_resources_create(VkDevice device, uint32_t queue_family_index,
                            FrameResources *f);

void frame_resources_destroy(FrameResources *f, VkDevice device);

bool frame_resources_submit(FrameResources *f, VkQueue graphics_queue);

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

bool swapchain_create(VkDevice device, VkPhysicalDevice gpu,
                      VkSurfaceKHR surface, uint32_t queue_family_index,
                      Swapchain *swapchain);

void swapchain_destroy(Swapchain *sc, VkDevice device);

bool swapchain_next_frame(Swapchain *sc, VkDevice device,
                          FrameResources **frame, Image **image,
                          uint32_t *image_index);

///////////////////////////////////////
/// Renderer
///////////////////////////////////////
typedef struct {
  SDL_Window *window;

  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  VkPhysicalDevice gpu;
  uint32_t queue_family_index;
  VkDevice device;
  VkQueue graphics_queue;

  Swapchain swapchain;
} Renderer;

// Renderer Core Functions

bool renderer_init(Renderer *r, RendererCreateInfo *c);
void renderer_shutdown(Renderer *r);

void renderer_draw(Renderer *r);

// Vulkan Initialization Functions

void vk_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *info);

bool vk_create_instance(VkInstance *instance, RendererCreateInfo *c);
bool vk_create_debug_messenger(VkInstance instance,
                               VkDebugUtilsMessengerEXT *debug_messenger);
bool vk_create_surface(VkInstance instance, SDL_Window *window,
                       VkSurfaceKHR *surface);

bool vk_gpu_suitable(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                     uint32_t *queue_family_index);
bool vk_select_gpu(VkInstance instance, VkSurfaceKHR surface,
                   VkPhysicalDevice *gpu, uint32_t *queue_family_index);

bool vk_create_device(VkInstance instance, VkPhysicalDevice gpu,
                      uint32_t queue_family_index, VkDevice *device,
                      VkQueue *graphics_queue);

bool vk_begin_command_buffer(VkCommandBuffer command);
