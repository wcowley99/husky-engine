#pragma once

#include "vk_base.h"

namespace Render {

class PipelineBuilder {
public:
  PipelineBuilder(vk::Device &device, vk::PipelineLayout &layout);

  PipelineBuilder &with_module(const std::string &filename,
                               vk::ShaderStageFlagBits stage);

  vk::Pipeline build_compute_pipeline();

private:
  vk::Device &device;
  vk::PipelineLayout &layout;

  std::vector<vk::ShaderModule> modules;
};

} // namespace Render
