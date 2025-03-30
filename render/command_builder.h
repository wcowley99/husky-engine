#pragma once

#include "vk_base.h"

#include <array>

namespace Render {

class CommandBuilder {
public:
  CommandBuilder(vk::CommandBuffer command, vk::Image active_image);

  CommandBuilder &begin();
  void end();

  CommandBuilder &with_active_image(
      vk::Image image,
      vk::ImageLayout current_layout = vk::ImageLayout::eUndefined);
  CommandBuilder &transition_to(vk::ImageLayout layout);
  CommandBuilder &clear(std::array<float, 4> color);

private:
  vk::CommandBuffer command;
  vk::Image active_image;
  vk::ImageLayout current_layout;
};

} // namespace Render
