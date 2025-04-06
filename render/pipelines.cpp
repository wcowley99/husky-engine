#include "pipelines.h"

#include <stdexcept>

#include "shader_sources.h"

namespace Render {

PipelineBuilder::PipelineBuilder(vk::Device &device, vk::PipelineLayout &layout)
    : device(device), layout(layout) {}

PipelineBuilder::~PipelineBuilder() {
  for (auto shader : modules) {
    device.destroyShaderModule(shader);
  }
}

PipelineBuilder &PipelineBuilder::with_module(const std::string &filename,
                                              vk::ShaderStageFlagBits stage) {
  auto &shader_bytes = lookup_shader_source(filename);
  std::cout << "got shader bytes!" << std::endl;

  vk::ShaderModuleCreateInfo create_info(
      {}, shader_bytes.size() * sizeof(uint32_t), shader_bytes.data());

  auto module = device.createShaderModule(create_info);
  std::cout << "created shader module!" << std::endl;
  this->modules.push_back(module);
  std::cout << "pushed module!" << std::endl;

  return *this;
}

vk::Pipeline PipelineBuilder::build_compute_pipeline() {
  if (modules.size() != 1) {
    throw std::runtime_error(
        "Compute Pipeline requires exactly 1 shader module.");
  }

  vk::PipelineShaderStageCreateInfo stage_info(
      {}, vk::ShaderStageFlagBits::eCompute, modules[0], "main");
  vk::ComputePipelineCreateInfo create_info({}, stage_info, layout);
  std::cout << "this far!" << std::endl;
  auto result = device.createComputePipelines(nullptr, create_info);
  std::cout << "past!" << std::endl;
  VK_ASSERT(result.result);

  return result.value[0];
}

} // namespace Render
