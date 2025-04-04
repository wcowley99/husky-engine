#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace Render {

void vk_assert(vk::Result result);

namespace VkUtil {

vk::ImageViewCreateInfo image_view_create_info(vk::Format format,
                                               vk::Image image,
                                               vk::ImageAspectFlags mask);

vk::Offset3D extent_to_offset(vk::Extent3D &extent);

} // namespace VkUtil

} // namespace Render
