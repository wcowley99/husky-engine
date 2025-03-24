#include "window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace Render {

Window::Window(const std::string &title, uint32_t width, uint32_t height) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(
        std::string("SDL could not initialize! SDL error: ") + SDL_GetError());
  }

  this->window =
      SDL_CreateWindow("Hello, World", 1920, 1080, SDL_WINDOW_VULKAN);

  if (!window) {
    throw std::runtime_error(
        std::string("SDL could not create window! SDL error: ") +
        SDL_GetError());
  }

  this->surface = SDL_GetWindowSurface(window);
}

Render::Window::~Window() {
  SDL_DestroyWindow(window);
  SDL_Quit();
}

vk::SurfaceKHR Window::create_surface(vk::Instance instance) {
  VkSurfaceKHR surface_handle;
  SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface_handle);
  return surface_handle;
}

std::vector<const char *> Window::required_instance_extensions() {
  uint32_t count = 0;
  auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);
  return std::vector<const char *>(sdl_extensions, sdl_extensions + count);
}

void Window::refresh() {
  SDL_FillSurfaceRect(surface, nullptr,
                      SDL_MapSurfaceRGB(surface, 0xFF, 0x00, 0x00));

  SDL_UpdateWindowSurface(window);
}

} // namespace Render
