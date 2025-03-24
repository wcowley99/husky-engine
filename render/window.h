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

  void refresh();

private:
  SDL_Window *window;
  SDL_Surface *surface;
};

} // namespace Render
