#pragma once

#include "vk_base.h"

#include <memory>
#include <set>

namespace Render {

class DescriptorLayoutBuilder {
public:
  DescriptorLayoutBuilder(vk::Device &device,
                          vk::ShaderStageFlagBits shader_stages);

  DescriptorLayoutBuilder &with_pNext(void *pNext);

  DescriptorLayoutBuilder &
  with_create_flags(vk::DescriptorSetLayoutCreateFlags flags);

  DescriptorLayoutBuilder &add_binding(uint32_t binding,
                                       vk::DescriptorType type);

  vk::DescriptorSetLayout build();

private:
  std::vector<vk::DescriptorSetLayoutBinding> bindings;
  void *pNext;
  vk::DescriptorSetLayoutCreateFlags create_flags;

  vk::Device &device;
  vk::ShaderStageFlags shader_stage_flags;
};

class DescriptorAllocator {
public:
  DescriptorAllocator(vk::Device &device, uint32_t max_sets,
                      const std::vector<vk::DescriptorPoolSize> &sizes);

  void reset();
  vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);

private:
  vk::UniqueDescriptorPool pool;
  vk::Device &device;
};

class DescriptorAllocatorBuilder {
public:
  DescriptorAllocatorBuilder(vk::Device &device);

  DescriptorAllocatorBuilder &with_descriptor_capacity(vk::DescriptorType type,
                                                       uint32_t capacity);

  std::unique_ptr<DescriptorAllocator> build();

private:
  struct DescriptorCapacity {
    vk::DescriptorType type;
    uint32_t capacity;

    bool operator<(const DescriptorCapacity &rhs) const;
  };

  vk::Device &device;

  std::set<DescriptorCapacity> capacities;
};

} // namespace Render
