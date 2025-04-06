#include "descriptors.h"

namespace Render {

// DescriptorLayoutBuilder
DescriptorLayoutBuilder::DescriptorLayoutBuilder(
    vk::Device &device, vk::ShaderStageFlagBits shader_stages)
    : device(device) {
  this->shader_stage_flags = shader_stages;
  this->pNext = nullptr;
}

DescriptorLayoutBuilder &DescriptorLayoutBuilder::with_pNext(void *pNext) {
  this->pNext = pNext;

  return *this;
}

DescriptorLayoutBuilder &DescriptorLayoutBuilder::with_create_flags(
    vk::DescriptorSetLayoutCreateFlags flags) {
  this->create_flags = flags;

  return *this;
}

DescriptorLayoutBuilder &
DescriptorLayoutBuilder::add_binding(uint32_t location,
                                     vk::DescriptorType type) {
  vk::DescriptorSetLayoutBinding binding(location, type, 1);
  this->bindings.emplace_back(binding);

  return *this;
}

vk::UniqueDescriptorSetLayout DescriptorLayoutBuilder::build() {
  for (auto &b : bindings) {
    b.stageFlags |= shader_stage_flags;
  }

  vk::DescriptorSetLayoutCreateInfo info(create_flags, bindings.size(),
                                         bindings.data(), pNext);

  vk::DescriptorSetLayout layout;
  VK_ASSERT(device.createDescriptorSetLayout(&info, nullptr, &layout));

  return vk::UniqueDescriptorSetLayout(layout);
}

// DescriptorAllocator
DescriptorAllocator::DescriptorAllocator(
    vk::Device &device, uint32_t max_sets,
    const std::vector<vk::DescriptorPoolSize> &sizes)
    : device(device) {
  vk::DescriptorPoolCreateInfo create_info({}, max_sets, sizes.size(),
                                           sizes.data());

  this->pool = device.createDescriptorPoolUnique(create_info);
}

void DescriptorAllocator::reset() { device.resetDescriptorPool(pool.get()); }

vk::DescriptorSet
DescriptorAllocator::allocate(vk::DescriptorSetLayout layout) {
  vk::DescriptorSetAllocateInfo alloc_info(pool.get(), 1, &layout);

  auto descriptor_sets = device.allocateDescriptorSets(alloc_info);

  return descriptor_sets[0];
}

// DescriptorAllocatorBuilder
DescriptorAllocatorBuilder::DescriptorAllocatorBuilder(vk::Device &device)
    : device(device) {}

DescriptorAllocatorBuilder &
DescriptorAllocatorBuilder::with_descriptor_capacity(vk::DescriptorType type,
                                                     uint32_t capacity) {
  this->capacities.insert(DescriptorCapacity{type, capacity});

  return *this;
}

std::unique_ptr<DescriptorAllocator> DescriptorAllocatorBuilder::build() {
  uint32_t max_sets = 0;
  std::vector<vk::DescriptorPoolSize> pool_sizes;
  for (auto &capacity : capacities) {
    pool_sizes.push_back(
        vk::DescriptorPoolSize(capacity.type, capacity.capacity));
    max_sets += capacity.capacity;
  }

  return std::make_unique<DescriptorAllocator>(device, max_sets, pool_sizes);
}

// DescriptorAllocatorBuilder::DescriptorCapacity
bool DescriptorAllocatorBuilder::DescriptorCapacity::operator<(
    const DescriptorCapacity &rhs) const {
  return type < rhs.type;
}

} // namespace Render
