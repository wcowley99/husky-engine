#pragma once

#include <memory>

#include "vk_base.h"

#include "image.h"

namespace Render {

class Swapchain {
public:
  Swapchain(vk::Device device, vk::SurfaceKHR surface, vk::PhysicalDevice gpu);

  vk::ResultValue<uint32_t> next_image_index(vk::Semaphore semaphore);
  Image &image(uint32_t index);

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
  std::vector<std::unique_ptr<Image>> swapchain_images;
};

} // namespace Render
