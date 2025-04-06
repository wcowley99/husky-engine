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

CommandBuilder &CommandBuilder::execute_compute(Image &image,
                                                vk::Pipeline &pipeline,
                                                vk::PipelineLayout &layout,
                                                vk::DescriptorSet descriptors) {
  transition_image(image, vk::ImageLayout::eGeneral);
  command.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);

  command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, 1,
                             &descriptors, 0, nullptr);

  command.dispatch(std::ceil(image.extent.width / 16.0f),
                   std::ceil(image.extent.height / 16.0f), 1);

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
