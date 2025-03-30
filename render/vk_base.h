#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

namespace Render {

constexpr uint32_t NUM_FRAMES = 3;

void vk_assert(vk::Result result);

void transition_image(vk::CommandBuffer cmd, vk::Image image,
                      vk::ImageLayout current, vk::ImageLayout next);

} // namespace Render
