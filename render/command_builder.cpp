#include "command_builder.h"

namespace Render {

CommandBuilder::CommandBuilder(vk::CommandBuffer command,
                               vk::Image active_image) {
  this->command = command;
  this->active_image = active_image;
  this->current_layout = vk::ImageLayout::eUndefined;
}

CommandBuilder &CommandBuilder::begin() {
  command.reset();
  vk::CommandBufferBeginInfo begin_info(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  command.begin(begin_info);

  return *this;
}

void CommandBuilder::end() { command.end(); }

CommandBuilder &
CommandBuilder::with_active_image(vk::Image image,
                                  vk::ImageLayout current_layout) {
  this->active_image = image;
  this->current_layout = current_layout;

  return *this;
}

CommandBuilder &CommandBuilder::transition_to(vk::ImageLayout new_layout) {
  auto aspect_mask = (new_layout == vk::ImageLayout::eDepthAttachmentOptimal)
                         ? vk::ImageAspectFlagBits::eDepth
                         : vk::ImageAspectFlagBits::eColor;
  vk::ImageMemoryBarrier2 imageBarrier(
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite,
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
      current_layout, new_layout, 0, 0, active_image,
      vk::ImageSubresourceRange(aspect_mask, 0, vk::RemainingMipLevels, 0,
                                vk::RemainingArrayLayers));

  this->current_layout = new_layout;

  vk::DependencyInfo dep_info;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &imageBarrier;
  command.pipelineBarrier2(dep_info);

  return *this;
}

CommandBuilder &CommandBuilder::clear(std::array<float, 4> color) {
  vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0,
                                        vk::RemainingMipLevels, 0,
                                        vk::RemainingArrayLayers);

  vk::ClearColorValue clear_color(color);
  command.clearColorImage(active_image, current_layout, &clear_color, 1,
                          &clear_range);

  return *this;
}

} // namespace Render
