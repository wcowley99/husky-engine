#pragma once

#include "vk_base.h"

#include <memory>

namespace Render {

class GraphicsPipeline {
public:
  GraphicsPipeline(vk::Device &device,
                   vk::GraphicsPipelineCreateInfo &create_info);

  vk::Pipeline &get_pipeline();
  vk::PipelineLayout &get_pipeline_layout();

private:
  vk::UniquePipeline pipeline;
  vk::UniquePipelineLayout layout;
};

class GraphicsPipelineBuilder {
public:
  GraphicsPipelineBuilder(vk::Device &device);

  GraphicsPipelineBuilder &with_vertex_shader(const std::string &filename);
  GraphicsPipelineBuilder &with_fragment_shader(const std::string &filename);

  GraphicsPipelineBuilder &with_topology(vk::PrimitiveTopology topology);
  GraphicsPipelineBuilder &with_polygon_mode(vk::PolygonMode mode);
  GraphicsPipelineBuilder &with_cull_mode(vk::CullModeFlags cull,
                                          vk::FrontFace face);

  GraphicsPipelineBuilder &with_color_attachment_format(vk::Format format);
  GraphicsPipelineBuilder &with_depth_attachment_format(vk::Format format);

  GraphicsPipelineBuilder &
  add_descriptor_layout(vk::DescriptorSetLayout &layout);
  GraphicsPipelineBuilder &add_push_constant(vk::ShaderStageFlagBits stage,
                                             uint32_t size);

  std::unique_ptr<GraphicsPipeline> build();

private:
  vk::Device &device;

  std::string vertex_shader;
  std::string fragment_shader;

  vk::PrimitiveTopology topology;
  vk::PolygonMode polygon_mode;
  vk::CullModeFlags cull_mode;
  vk::FrontFace front_face;

  vk::Format color_attachment_format;
  vk::Format depth_attachment_format;

  std::vector<vk::DescriptorSetLayout> descriptors;
  std::vector<vk::PushConstantRange> push_constants;
};

} // namespace Render
