#pragma once

#include "platform.h"

#include "vkb.h"

#include <SDL3/SDL.h>

typedef struct {
        VkInstance instance;
        VkSurfaceKHR surface;
        VkDebugUtilsMessengerEXT debug_messenger;

        VkPhysicalDevice physical_device;

        VkDevice device;
        uint32_t queue_family_index;
        VkQueue graphics_queue;
} device_t;

device_t device_create();
void device_shutdown(device_t *device);
