#include "allocator.h"

namespace Render {

Allocator::Allocator(vk::PhysicalDevice gpu, vk::Device &device,
                     vk::Instance &instance) {
  VmaAllocatorCreateInfo create_info = {};
  create_info.physicalDevice = gpu;
  create_info.device = device;
  create_info.instance = instance;
  create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  const auto &d = VULKAN_HPP_DEFAULT_DISPATCHER;
  VmaVulkanFunctions func = {};
  func.vkGetInstanceProcAddr = d.vkGetInstanceProcAddr;
  func.vkGetDeviceProcAddr = d.vkGetDeviceProcAddr;

  create_info.pVulkanFunctions = &func;

  vmaCreateAllocator(&create_info, &this->gpu_allocator);
}

Allocator::~Allocator() { vmaDestroyAllocator(gpu_allocator); }

} // namespace Render
