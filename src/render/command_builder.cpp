#include "command_builder.h"

#include <cmath>
#include <iostream>

namespace Render {

CommandBuilder::CommandBuilder(vk::CommandBuffer command) {
  this->command = command;

  command.reset();
  vk::CommandBufferBeginInfo begin_info(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  command.begin(begin_info);
}

void CommandBuilder::present(Image &image) {
  transition_image(image, vk::ImageLayout::ePresentSrcKHR);
  command.end();
}

void CommandBuilder::transition_image(Image &image,
                                      vk::ImageLayout new_layout) {
  auto aspect_mask = (new_layout == vk::ImageLayout::eDepthAttachmentOptimal)
                         ? vk::ImageAspectFlagBits::eDepth
                         : vk::ImageAspectFlagBits::eColor;
  vk::ImageMemoryBarrier2 imageBarrier(
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite,
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
      image.layout, new_layout, 0, 0, image.image,
      vk::ImageSubresourceRange(aspect_mask, 0, vk::RemainingMipLevels, 0,
                                vk::RemainingArrayLayers));

  image.layout = new_layout;

  vk::DependencyInfo dep_info;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &imageBarrier;
  command.pipelineBarrier2(dep_info);
}

CommandBuilder &CommandBuilder::begin_rendering(Image &image) {
  transition_image(image, vk::ImageLayout::eColorAttachmentOptimal);

  vk::RenderingAttachmentInfo color_attachment(
      *image.image_view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {});

  vk::Extent2D extent(image.extent.width, image.extent.height);
  vk::RenderingInfo render_info({}, vk::Rect2D(vk::Offset2D{0, 0}, extent), 1,
                                {}, 1, &color_attachment, nullptr);

  command.beginRendering(&render_info);

  vk::Viewport viewport(0.0f, 0.0f, image.extent.width, image.extent.height,
                        0.0f, 1.0f);
  vk::Rect2D scissor(vk::Offset2D(0, 0), extent);

  command.setViewport(0, 1, &viewport);
  command.setScissor(0, 1, &scissor);

  return *this;
}

CommandBuilder &CommandBuilder::end_rendering() {
  command.endRendering();

  return *this;
}

CommandBuilder &CommandBuilder::clear(Image &image,
                                      std::array<float, 4> color) {
  transition_image(image, vk::ImageLayout::eGeneral);
  vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0,
                                        vk::RemainingMipLevels, 0,
                                        vk::RemainingArrayLayers);

  vk::ClearColorValue clear_color(color);
  command.clearColorImage(image.image, image.layout, &clear_color, 1,
                          &clear_range);

  return *this;
}

CommandBuilder &
CommandBuilder::execute_compute(Image &image, ComputePipeline &compute,
                                vk::DescriptorSet descriptors,
                                const ComputePushConstant &push_constant) {
  transition_image(image, vk::ImageLayout::eGeneral);
  command.bindPipeline(vk::PipelineBindPoint::eCompute, compute.get_pipeline());

  command.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                             compute.get_layout(), 0, 1, &descriptors, 0,
                             nullptr);

  command.pushConstants(compute.get_layout(), vk::ShaderStageFlagBits::eCompute,
                        0, sizeof(ComputePushConstant), &push_constant);

  command.dispatch(std::ceil(image.extent.width / 16.0f),
                   std::ceil(image.extent.height / 16.0f), 1);

  return *this;
}

CommandBuilder &CommandBuilder::execute_graphics(Image &image,
                                                 GraphicsPipeline &graphics) {
  command.bindPipeline(vk::PipelineBindPoint::eGraphics,
                       graphics.get_pipeline());

  command.draw(3, 1, 0, 0);

  return *this;
}

CommandBuilder &CommandBuilder::draw_mesh(Image &image,
                                          GraphicsPipeline &graphics,
                                          MeshBuffer &mesh_buffer) {
  command.bindPipeline(vk::PipelineBindPoint::eGraphics,
                       graphics.get_pipeline());

  MeshPushConstant pc;
  pc.worldMatrix = glm::mat4(1.0f);
  pc.vertexBuffer = mesh_buffer.get_vertex_buffer_address();

  command.pushConstants(graphics.get_pipeline_layout(),
                        vk::ShaderStageFlagBits::eVertex, 0,
                        sizeof(MeshPushConstant), &pc);
  command.bindIndexBuffer(mesh_buffer.get_index_buffer().get_buffer(), 0,
                          vk::IndexType::eUint32);
  command.drawIndexed(6, 1, 0, 0, 0);

  return *this;
}

CommandBuilder &CommandBuilder::draw_imgui(Image &image,
                                           vk::ImageView image_view) {
  transition_image(image, vk::ImageLayout::eColorAttachmentOptimal);

  vk::RenderingAttachmentInfo color_attachment(
      image_view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {});

  vk::Extent2D extent(image.extent.width, image.extent.height);
  vk::RenderingInfo render_info({}, vk::Rect2D(vk::Offset2D{0, 0}, extent), 1,
                                {}, 1, &color_attachment, nullptr);

  command.beginRendering(&render_info);

  auto draw_data = ImGui::GetDrawData();
  ImGui_ImplVulkan_RenderDrawData(draw_data, command);

  command.endRendering();

  return *this;
}

CommandBuilder &CommandBuilder::copy_to(Image &src, Image &dst) {
  transition_image(src, vk::ImageLayout::eTransferSrcOptimal);
  transition_image(dst, vk::ImageLayout::eTransferDstOptimal);

  auto subresource =
      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);

  std::array<vk::Offset3D, 2> src_offsets = {
      vk::Offset3D{}, VkUtil::extent_to_offset(src.extent)};
  std::array<vk::Offset3D, 2> dst_offsets = {
      vk::Offset3D{}, VkUtil::extent_to_offset(dst.extent)};
  auto blit =
      vk::ImageBlit2(subresource, src_offsets, subresource, dst_offsets);

  auto blit_info =
      vk::BlitImageInfo2(src.image, src.layout, dst.image, dst.layout, 1, &blit,
                         vk::Filter::eLinear);
  command.blitImage2(blit_info);

  return *this;
}

} // namespace Render
