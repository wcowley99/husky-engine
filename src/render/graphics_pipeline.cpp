#include "graphics_pipeline.h"

namespace Render {

// GraphicsPipeline
GraphicsPipeline::GraphicsPipeline(
    vk::Device &device, vk::GraphicsPipelineCreateInfo &create_info) {
  this->layout = vk::UniquePipelineLayout(create_info.layout, device);
  this->pipeline =
      device.createGraphicsPipelineUnique(nullptr, create_info).value;
}

vk::Pipeline &GraphicsPipeline::get_pipeline() { return this->pipeline.get(); }

// GraphicsPipelineBuilder
GraphicsPipelineBuilder::GraphicsPipelineBuilder(vk::Device &device)
    : device(device) {
  this->topology = vk::PrimitiveTopology::eTriangleList;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_vertex_shader(const std::string &filename) {
  this->vertex_shader = filename;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_fragment_shader(const std::string &filename) {
  this->fragment_shader = filename;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_topology(vk::PrimitiveTopology topology) {
  this->topology = topology;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_polygon_mode(vk::PolygonMode mode) {
  this->polygon_mode = mode;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_cull_mode(vk::CullModeFlags cull,
                                        vk::FrontFace face) {
  this->cull_mode = cull;
  this->front_face = face;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_color_attachment_format(vk::Format format) {
  this->color_attachment_format = format;

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::with_depth_attachment_format(vk::Format format) {
  this->depth_attachment_format = format;

  return *this;
}

GraphicsPipelineBuilder &GraphicsPipelineBuilder::add_descriptor_layout(
    vk::DescriptorSetLayout &layout) {
  this->descriptors.push_back(layout);

  return *this;
}

GraphicsPipelineBuilder &
GraphicsPipelineBuilder::add_push_constant(vk::ShaderStageFlagBits stage,
                                           uint32_t size) {
  this->push_constants.emplace_back(stage, 0, size);

  return *this;
}

std::unique_ptr<GraphicsPipeline> GraphicsPipelineBuilder::build() {
  // With dyanmic viewport state we don't need to specify the viewport and
  // scissor now.
  vk::PipelineViewportStateCreateInfo viewport_state({}, 1, nullptr, 1,
                                                     nullptr);

  vk::PipelineColorBlendAttachmentState color_blend_attachment(vk::False);
  color_blend_attachment.colorWriteMask =
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  vk::PipelineColorBlendStateCreateInfo color_blending(
      {}, vk::False, vk::LogicOp::eCopy, color_blend_attachment);

  vk::PipelineDepthStencilStateCreateInfo depth_testing(
      {}, vk::False, vk::False, vk::CompareOp::eNever, vk::False, vk::False, {},
      {}, 0.0f, 1.0f);

  vk::PipelineVertexInputStateCreateInfo vertex_input;

  vk::DynamicState state[] = {vk::DynamicState::eViewport,
                              vk::DynamicState::eScissor};

  auto vertex = VkUtil::create_shader_module(device, vertex_shader);
  auto fragment = VkUtil::create_shader_module(device, fragment_shader);

  std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
      {{}, vk::ShaderStageFlagBits::eVertex, vertex.get(), "main"},
      {{}, vk::ShaderStageFlagBits::eFragment, fragment.get(), "main"}};

  vk::PipelineInputAssemblyStateCreateInfo input_assembly({}, topology,
                                                          vk::False);

  vk::PipelineRasterizationStateCreateInfo rasterizer(
      {}, vk::False, vk::False, polygon_mode, cull_mode, front_face, vk::False,
      0.0f, 0.0f, 0.0f, 1.0f);

  vk::PipelineMultisampleStateCreateInfo multisample(
      {}, vk::SampleCountFlagBits::e1, vk::False, 1.0f, nullptr, vk::False,
      vk::False);

  vk::PipelineDynamicStateCreateInfo dynamic_state({}, 2, state);

  vk::PipelineRenderingCreateInfo render_info({}, 1, &color_attachment_format,
                                              depth_attachment_format);

  auto layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo(
      {}, descriptors.size(), descriptors.data(), push_constants.size(),
      push_constants.data()));

  vk::GraphicsPipelineCreateInfo pipeline_info(
      {}, shader_stages.size(), shader_stages.data(), &vertex_input,
      &input_assembly, nullptr, &viewport_state, &rasterizer, &multisample,
      &depth_testing, &color_blending, &dynamic_state, layout, {}, {}, {}, {},
      &render_info);

  return std::make_unique<GraphicsPipeline>(device, pipeline_info);
}

} // namespace Render
