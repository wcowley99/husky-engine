#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <stdint.h>

typedef struct {
  uint32_t width;
  uint32_t height;
  const char *title;
  bool debug;
} RendererCreateInfo;

typedef struct {
  // SDL handles
  SDL_Window *window;

  // Vulkan handles
  VkInstance instance;
} Renderer;

// Renderer Core Functions

bool renderer_init(Renderer *r, RendererCreateInfo *c);
void renderer_shutdown(Renderer *r);

void renderer_draw(Renderer *r);

// Vulkan Initialization Functions

bool vk_create_instance(VkInstance *instance, RendererCreateInfo *c);
