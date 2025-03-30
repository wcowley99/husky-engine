#include "swapchain.h"

#include <algorithm>
#include <iostream>

namespace Render {

Swapchain::Swapchain(vk::Device device, vk::SurfaceKHR surface,
                     vk::PhysicalDevice gpu) {
  this->device = device;
  this->gpu = gpu;
  this->surface = surface;

  this->init();
}

vk::ResultValue<uint32_t> Swapchain::next_image_index(vk::Semaphore semaphore) {
  return device.acquireNextImageKHR(swapchain.get(), 1000000000, semaphore);
}

vk::Image Swapchain::image(uint32_t index) { return swapchain_images[index]; }

vk::PresentInfoKHR Swapchain::get_present_info(vk::Semaphore *semaphore,
                                               uint32_t *image_index) {

  return vk::PresentInfoKHR(1, semaphore, 1, &swapchain.get(), image_index);
}

void Swapchain::recreate() {
  std::cout << "Recreating Swapchain" << std::endl;
  device.waitIdle();
  this->init();
}

uint32_t Swapchain::num_images() const { return swapchain_images.size(); }

void Swapchain::init() {
  auto capabilities = gpu.getSurfaceCapabilitiesKHR(surface);
  auto formats = gpu.getSurfaceFormatsKHR(surface);

  auto format = vk::Format::eB8G8R8A8Srgb;
  uint32_t num_images = std::max(capabilities.minImageCount + 1, 3u);

  vk::SwapchainCreateInfoKHR swapchainCreateInfo(
      {}, surface, NUM_FRAMES, format, vk::ColorSpaceKHR::eSrgbNonlinear,
      capabilities.currentExtent, 1,
      vk::ImageUsageFlagBits::eColorAttachment |
          vk::ImageUsageFlagBits::eTransferDst,
      vk::SharingMode::eExclusive, 0, nullptr,
      vk::SurfaceTransformFlagBitsKHR::eIdentity,
      vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::PresentModeKHR::eFifo, true,
      nullptr);
  this->swapchain = device.createSwapchainKHRUnique(swapchainCreateInfo);
  this->swapchain_images = device.getSwapchainImagesKHR(swapchain.get());

  for (auto image : swapchain_images) {
    vk::ImageViewCreateInfo image_view_create_info(
        {}, image, vk::ImageViewType::e2D, format,
        vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                             vk::ComponentSwizzle::eB,
                             vk::ComponentSwizzle::eA},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    swapchain_image_views.push_back(
        device.createImageViewUnique(image_view_create_info));
  }
}

} // namespace Render
