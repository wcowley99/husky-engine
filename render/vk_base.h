#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

namespace Render {

void vk_assert(vk::Result result);

} // namespace Render
