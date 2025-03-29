#pragma once

#include "canvas.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace Render {

class Window : public Canvas {
public:
  Window(const std::string &title, uint32_t width, uint32_t height);
  ~Window();

  virtual vk::SurfaceKHR create_surface(vk::Instance instance) override;
  virtual std::vector<const char *> required_instance_extensions() override;
  virtual vk::Extent2D extent() override;

  void refresh();

private:
  uint32_t width;
  uint32_t height;
  SDL_Window *window;
};

} // namespace Render
