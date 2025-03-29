#include "vk_base.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Render {

void vk_assert(vk::Result result) {
  if (result != vk::Result::eSuccess) {
    throw std::runtime_error("Assertion on a VkResult failed.");
  }
}

void transition_image(vk::CommandBuffer cmd, vk::Image image,
                      vk::ImageLayout current, vk::ImageLayout next) {
  auto aspect_mask = (next == vk::ImageLayout::eDepthAttachmentOptimal)
                         ? vk::ImageAspectFlagBits::eDepth
                         : vk::ImageAspectFlagBits::eColor;
  vk::ImageMemoryBarrier2 imageBarrier(
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite,
      vk::PipelineStageFlagBits2::eAllCommands,
      vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
      current, next, 0, 0, image,
      vk::ImageSubresourceRange(aspect_mask, 0, vk::RemainingMipLevels, 0,
                                vk::RemainingArrayLayers));

  vk::DependencyInfo dep_info;
  dep_info.imageMemoryBarrierCount = 1;
  dep_info.pImageMemoryBarriers = &imageBarrier;
  cmd.pipelineBarrier2(dep_info);
}

} // namespace Render
