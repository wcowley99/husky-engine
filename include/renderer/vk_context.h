#pragma once

#include "platform.h"

#include "vkb.h"

#include <SDL3/SDL.h>

void vk_context_init();
void vk_context_shutdown();

void vk_context_wait_idle();

VkInstance vk_context_instance();
VkSurfaceKHR vk_context_surface();
VkPhysicalDevice vk_context_physical_device();
VkDevice vk_context_device();
uint32_t vk_context_queue_family_index();
VkQueue vk_context_graphics_queue();
