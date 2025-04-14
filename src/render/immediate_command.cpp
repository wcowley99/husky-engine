#include "immediate_command.h"

namespace Render {

ImmediateCommand::ImmediateCommand(vk::Device &device, vk::Queue &queue,
                                   uint32_t queue_family_index)
    : device(device), queue(queue) {
  this->fence = device.createFenceUnique(vk::FenceCreateInfo());

  vk::CommandPoolCreateInfo command_pool_info(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index);

  this->pool = device.createCommandPoolUnique(command_pool_info);
  vk::CommandBufferAllocateInfo alloc_info(this->pool.get(),
                                           vk::CommandBufferLevel::ePrimary, 1);
  this->command = device.allocateCommandBuffers(alloc_info)[0];
}

void ImmediateCommand::submit(
    std::function<void(vk::CommandBuffer &)> &&function) {
  vk::CommandBufferBeginInfo info(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

  VK_ASSERT(command.begin(&info));

  function(this->command);

  command.end();

  vk::SubmitInfo submit_info(0, nullptr, nullptr, 1, &command, 0, nullptr);

  VK_ASSERT(queue.submit(1, &submit_info, fence.get()));
  VK_ASSERT(device.waitForFences(fence.get(), vk::True, 1000000000));

  device.resetFences(fence.get());
  device.resetCommandPool(pool.get());
}

} // namespace Render
