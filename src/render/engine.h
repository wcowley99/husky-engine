#pragma once

#include "canvas.h"
#include "frames.h"
#include "swapchain.h"

#include "vk_base.h"

#include "allocator.h"
#include "compute_pipeline.h"
#include "descriptors.h"
#include "graphics_pipeline.h"
#include "image.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include <optional>

namespace Render {

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

class Engine {
public:
  Engine(Canvas &canvas);
  ~Engine();

  void draw();

private:
  std::pair<vk::PhysicalDevice, uint32_t>
  select_device(const std::vector<vk::PhysicalDevice> &devices) const;

  bool has_required_vulkan(vk::PhysicalDevice device) const;
  bool has_required_features(vk::PhysicalDevice device) const;
  std::optional<uint32_t>
  find_suitable_queue_family(vk::PhysicalDevice device) const;

  void init_instance(Canvas &canvas);
  void init_device();
  void init_descriptors();
  void init_pipelines();
  void init_imgui(Canvas &canvas);
  vk::UniqueDevice create_device();

private:
  vk::UniqueInstance instance;
  vk::UniqueSurfaceKHR surface;
  vk::UniqueDevice device;
  vk::PhysicalDevice gpu;
  vk::Queue graphics_queue;
  uint32_t queue_family_index;
  std::unique_ptr<Swapchain> swapchain;

  std::unique_ptr<FrameQueue> frames;
  uint32_t frame_count = 0;

  std::shared_ptr<Allocator> allocator;

  std::unique_ptr<Image> draw_image;

  std::unique_ptr<DescriptorAllocator> global_descriptor_allocator;
  vk::DescriptorSet draw_image_descriptors;
  vk::UniqueDescriptorSetLayout draw_image_descriptor_layouts;

  std::unique_ptr<ComputePipeline> gradient_compute;
  std::unique_ptr<GraphicsPipeline> triangle_pipeline;

  std::unique_ptr<DescriptorAllocator> imgui_descriptor_allocator;
};

} // namespace Render
