#include "window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace Render {

Window::Window(const std::string &title, uint32_t width, uint32_t height) {
  this->width = width;
  this->height = height;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(
        std::string("SDL could not initialize! SDL error: ") + SDL_GetError());
  }

  this->window = SDL_CreateWindow("Hello, World", width, height,
                                  SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  if (!window) {
    throw std::runtime_error(
        std::string("SDL could not create window! SDL error: ") +
        SDL_GetError());
  }
}

Render::Window::~Window() {
  std::cout << "destroying window" << std::endl;
  SDL_DestroyWindow(window);
  SDL_Quit();
}

vk::SurfaceKHR Window::create_surface(vk::Instance instance) {
  VkSurfaceKHR surface_handle;
  if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface_handle)) {
    throw std::runtime_error(
        std::string("SDL could not create window! SDL error: ") +
        SDL_GetError());
  }

  return surface_handle;
}

std::vector<const char *> Window::required_instance_extensions() {
  uint32_t count = 0;
  auto sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);
  return std::vector<const char *>(sdl_extensions, sdl_extensions + count);
}

vk::Extent2D Window::extent() { return vk::Extent2D{width, height}; }

void Window::refresh() { SDL_UpdateWindowSurface(window); }

} // namespace Render
