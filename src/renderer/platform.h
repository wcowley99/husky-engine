#pragma once

#include "vkb.h"

#include <SDL3/SDL.h>

void platform_init(uint32_t width, uint32_t height, const char *title);
void platform_shutdown(void);

VkSurfaceKHR platform_create_surface(VkInstance instance);

void platform_get_size(uint32_t *width, uint32_t *height);
bool platform_size_changed();

void platform_update_window();
