#pragma once

#include "vk_base.h"

#include <memory>

namespace Render {

class ComputePipeline {
public:
  ComputePipeline(vk::Device &device, const std::string &filename,
                  const std::vector<vk::DescriptorSetLayout> &descriptors,
                  const std::vector<vk::PushConstantRange> &push_constants);

  vk::Pipeline &get_pipeline();
  vk::PipelineLayout &get_layout();

private:
  vk::UniquePipeline pipeline;
  vk::UniquePipelineLayout layout;
};

class ComputePipelineBuilder {
public:
  ComputePipelineBuilder(vk::Device &device, const std::string &filename);

  std::unique_ptr<ComputePipeline> build();

  ComputePipelineBuilder &
  add_descriptor_layout(vk::DescriptorSetLayout &layout);

  ComputePipelineBuilder &add_push_constant(uint32_t size);

private:
  vk::Device &device;
  const std::string &filename;

  std::vector<vk::DescriptorSetLayout> layouts;
  std::vector<uint32_t> push_constant_sizes;
};

} // namespace Render
