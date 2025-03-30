#include "frames.h"

namespace Render {

vk::Result FrameContext::show(Swapchain &swapchain, vk::Queue graphics_queue,
                              uint32_t image_index) {
  vk::CommandBufferSubmitInfo submit_info(buffer);
  vk::SemaphoreSubmitInfo wait_info(
      swapchain_semaphore.get(), 1,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  vk::SemaphoreSubmitInfo signal_info(render_semaphore.get(), 1,
                                      vk::PipelineStageFlagBits2::eAllGraphics);
  vk::SubmitInfo2 submit({}, 1, &wait_info, 1, &submit_info, 1, &signal_info);
  if (graphics_queue.submit2(1, &submit, render_fence.get()) !=
      vk::Result::eSuccess) {
    throw std::runtime_error("Failed Submit2");
  }

  auto present =
      swapchain.get_present_info(&render_semaphore.get(), &image_index);
  return graphics_queue.presentKHR(present);
}

FrameQueue::FrameQueue(vk::Device &device, uint32_t queue_family_index,
                       uint32_t num_frames) {
  this->index = 0;
  vk::CommandPoolCreateInfo command_pool_info(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index);

  vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);
  vk::SemaphoreCreateInfo semaphore_info;
  frames.resize(num_frames);
  for (auto &frame_data : frames) {
    frame_data.pool = device.createCommandPoolUnique(command_pool_info);
    vk::CommandBufferAllocateInfo alloc_info(
        frame_data.pool.get(), vk::CommandBufferLevel::ePrimary, 1);
    frame_data.buffer = device.allocateCommandBuffers(alloc_info)[0];
    frame_data.render_fence = device.createFenceUnique(fence_info);
    frame_data.swapchain_semaphore =
        device.createSemaphoreUnique(semaphore_info);
    frame_data.render_semaphore = device.createSemaphoreUnique(semaphore_info);
  }
}

FrameContext &FrameQueue::next() {
  auto &frame = frames[index];
  index = (index + 1) % frames.size();

  return frame;
}

CommandBuilder FrameContext::command_builder(vk::Image active_image) {
  return CommandBuilder(buffer, active_image);
}

} // namespace Render
