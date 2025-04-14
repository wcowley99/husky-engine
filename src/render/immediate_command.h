#pragma once

#include "vk_base.h"

namespace Render {

class ImmediateCommand {
public:
  ImmediateCommand(vk::Device &device, vk::Queue &queue,
                   uint32_t queue_family_index);

  void submit(std::function<void(vk::CommandBuffer &)> &&function);

private:
  vk::UniqueCommandPool pool;
  vk::CommandBuffer command;
  vk::UniqueFence fence;

  vk::Queue &queue;
  vk::Device &device;
};

} // namespace Render
