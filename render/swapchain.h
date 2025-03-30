#pragma once

#include <memory>

#include "vk_base.h"

namespace Render {

class Swapchain {
public:
  Swapchain(vk::Device device, vk::SurfaceKHR surface, vk::PhysicalDevice gpu);

  vk::ResultValue<uint32_t> next_image_index(vk::Semaphore semaphore);
  vk::Image image(uint32_t index);

  vk::PresentInfoKHR get_present_info(vk::Semaphore *semaphore,
                                      uint32_t *image_index);

  void recreate();

  uint32_t num_images() const;

private:
  void init();

private:
  vk::Device device;
  vk::PhysicalDevice gpu;
  vk::SurfaceKHR surface;

  vk::UniqueSwapchainKHR swapchain;
  std::vector<vk::Image> swapchain_images;
  std::vector<vk::UniqueImageView> swapchain_image_views;
};

} // namespace Render
