#include "vk_base.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Render {

void vk_assert(vk::Result result) {
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error("Assertion on a VkResult failed.");
  }
}

} // namespace Render
