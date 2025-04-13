#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>

#define VK_ASSERT(x)                                                           \
  do {                                                                         \
    auto err = static_cast<VkResult>(x);                                       \
    if (err) {                                                                 \
      std::cout << "[" << __FILE__ << ":" << __LINE__ << "] "                  \
                << string_VkResult(err);                                       \
    }                                                                          \
  } while (false)

namespace Render {

struct ComputePushConstant {
  glm::vec4 top_color;
  glm::vec4 bottom_color;
  float padding[8];
};

namespace VkUtil {

vk::ImageViewCreateInfo image_view_create_info(vk::Format format,
                                               vk::Image image,
                                               vk::ImageAspectFlags mask);

vk::Offset3D extent_to_offset(vk::Extent3D &extent);

vk::UniqueShaderModule create_shader_module(vk::Device &device,
                                            const std::string &filename);

} // namespace VkUtil

} // namespace Render
