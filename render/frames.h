#pragma once

#include "command_builder.h"
#include "swapchain.h"
#include "vk_base.h"

namespace Render {

class FrameContext {
public:
  CommandBuilder command_builder(vk::Image active_image);

  vk::Result show(Swapchain &swapchain, vk::Queue graphics_queue,
                  uint32_t image_index);
  vk::UniqueCommandPool pool;
  vk::CommandBuffer buffer;
  vk::UniqueSemaphore swapchain_semaphore, render_semaphore;
  vk::UniqueFence render_fence;
};

class FrameQueue {
public:
  FrameQueue(vk::Device &device, uint32_t queue_family_index,
             uint32_t num_frames);

  FrameContext &next();

private:
  std::vector<FrameContext> frames;
  uint32_t index;
};

} // namespace Render
