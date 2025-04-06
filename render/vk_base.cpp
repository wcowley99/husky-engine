#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vk_base.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Render {

namespace VkUtil {

vk::ImageViewCreateInfo image_view_create_info(vk::Format format,
                                               vk::Image image,
                                               vk::ImageAspectFlags mask) {
  return vk::ImageViewCreateInfo(
      {}, image, vk::ImageViewType::e2D, format,
      vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                           vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
      vk::ImageSubresourceRange{mask, 0, 1, 0, 1});
}

vk::Offset3D extent_to_offset(vk::Extent3D &extent) {
  return vk::Offset3D{static_cast<int32_t>(extent.width),
                      static_cast<int32_t>(extent.height),
                      static_cast<int32_t>(extent.depth)};
}

} // namespace VkUtil

} // namespace Render
