#pragma once

#include "vk_base.h"

#include "buffer.h"
#include "compute_pipeline.h"
#include "graphics_pipeline.h"
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

  CommandBuilder &begin_rendering(Image &image);
  CommandBuilder &end_rendering();

  CommandBuilder &execute_compute(Image &image, ComputePipeline &compute,
                                  vk::DescriptorSet descriptors,
                                  const ComputePushConstant &push_constant);
  CommandBuilder &execute_graphics(Image &image, GraphicsPipeline &graphics);
  CommandBuilder &draw_mesh(Image &image, GraphicsPipeline &graphics,
                            MeshBuffer &mesh_buffer);
  CommandBuilder &draw_imgui(Image &image, vk::ImageView image_view);

private:
  void transition_image(Image &image, vk::ImageLayout layout);

private:
  vk::CommandBuffer command;
};

} // namespace Render
