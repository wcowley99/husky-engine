#include "renderer.h"

#include <SDL3/SDL_vulkan.h>

#include <stdio.h>

bool renderer_init(Renderer *r, RendererCreateInfo *c) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    printf("Failed to initialize SDL: %s\n", SDL_GetError());
    return false;
  }

  r->window = SDL_CreateWindow(c->title, c->width, c->height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
  if (!r->window) {
    printf("Failed to create Window: %s\n", SDL_GetError());
    return false;
  }

  if (!vk_create_instance(&r->instance, c)) {
    printf("Failed to initialize vulkan instance.");
    return false;
  }

  return true;
}

void renderer_shutdown(Renderer *r) {
  SDL_DestroyWindow(r->window);
  SDL_Quit();
}

void renderer_draw(Renderer *r) { SDL_UpdateWindowSurface(r->window); }

bool vk_create_instance(VkInstance *instance, RendererCreateInfo *c) {
  VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                .pApplicationName = c->title,
                                .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
                                .pEngineName = "No Engine",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};
  uint32_t count = 0;
  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&count);

  printf("SDL Extensions:\n");
  for (uint32_t i = 0; i < count; i += 1) {
    printf("  - %s\n", extensions[i]);
  }

  const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

  VkInstanceCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = count,
      .ppEnabledExtensionNames = extensions,
      .enabledLayerCount = 1,
      .ppEnabledLayerNames = layers};

  VkResult result = vkCreateInstance(&create_info, NULL, instance);

  return result == VK_SUCCESS;
}
