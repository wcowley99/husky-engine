#pragma once

#include <vector>

#include "vk_base.h"

namespace Render {

class Canvas {
public:
  Canvas() = default;
  virtual ~Canvas() = default;

  virtual vk::SurfaceKHR create_surface(vk::Instance instance) = 0;
  virtual std::vector<const char *> required_instance_extensions() = 0;
  virtual vk::Extent2D extent() = 0;
  virtual void init_imgui() = 0;
};

} // namespace Render
