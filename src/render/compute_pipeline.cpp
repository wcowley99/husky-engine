#include "compute_pipeline.h"

namespace Render {

// ComputePipeline
ComputePipeline::ComputePipeline(
    vk::Device &device, const std::string &filename,
    const std::vector<vk::DescriptorSetLayout> &descriptors,
    const std::vector<vk::PushConstantRange> &push_constants) {
  auto module = VkUtil::create_shader_module(device, filename);

  vk::PushConstantRange push_constant(vk::ShaderStageFlagBits::eCompute, 0,
                                      sizeof(ComputePushConstant));
  this->layout = device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo(
      {}, descriptors.size(), descriptors.data(), push_constants.size(),
      push_constants.data()));

  vk::PipelineShaderStageCreateInfo stage_info(
      {}, vk::ShaderStageFlagBits::eCompute, *module, "main");
  vk::ComputePipelineCreateInfo compute_info({}, stage_info, layout.get());

  this->pipeline = device.createComputePipelineUnique({}, compute_info).value;
}

vk::Pipeline &ComputePipeline::get_pipeline() { return this->pipeline.get(); }

vk::PipelineLayout &ComputePipeline::get_layout() { return this->layout.get(); }

// ComputePipelineBuilder
ComputePipelineBuilder::ComputePipelineBuilder(vk::Device &device,
                                               const std::string &filename)
    : device(device), filename(filename) {}

std::unique_ptr<ComputePipeline> ComputePipelineBuilder::build() {
  std::vector<vk::PushConstantRange> push_constants;
  for (auto size : push_constant_sizes) {
    push_constants.emplace_back(vk::ShaderStageFlagBits::eCompute, 0, size);
  }

  return std::make_unique<ComputePipeline>(device, filename, layouts,
                                           push_constants);
}

ComputePipelineBuilder &
ComputePipelineBuilder::add_descriptor_layout(vk::DescriptorSetLayout &layout) {
  this->layouts.push_back(layout);

  return *this;
}

ComputePipelineBuilder &
ComputePipelineBuilder::add_push_constant(uint32_t size) {
  this->push_constant_sizes.push_back(size);

  return *this;
}

} // namespace Render
