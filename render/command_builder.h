#pragma once

#include "vk_base.h"

#include "image.h"

#include "imgui_impl_vulkan.h"

#include <array>

namespace Render {

class CommandBuilder {
public:
  CommandBuilder(vk::CommandBuffer command);

  void present(Image &image);

  CommandBuilder &clear(Image &image, std::array<float, 4> color);
  CommandBuilder &copy_to(Image &src, Image &dst);

  CommandBuilder &execute_compute(Image &image, vk::Pipeline &pipeline,
                                  vk::PipelineLayout &layout,
                                  vk::DescriptorSet descriptors);
  CommandBuilder &draw_imgui(Image &image, vk::ImageView image_view);

private:
  void transition_image(Image &image, vk::ImageLayout layout);

private:
  vk::CommandBuffer command;
};

} // namespace Render
