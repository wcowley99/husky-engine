#pragma once

#include "vk_base.h"

namespace Render {

class Allocator {
public:
  Allocator(vk::PhysicalDevice gpu, vk::Device &device, vk::Instance &instance);
  ~Allocator();

  VmaAllocator gpu_allocator;
};

} // namespace Render
