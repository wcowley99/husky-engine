#pragma once

#include "descriptors.h"
#include "image.h"
#include "vkb.h"

#include <stdbool.h>

typedef enum {
        FRAME_BUFFER_CAMERA,
        FRAME_BUFFER_INSTANCES,
} frame_buffer_type_t;

bool swapchain_create();
void swapchain_destroy();
void SwapchainRecreate();

bool swapchain_next_frame();

void swapchain_current_frame_submit();
void swapchain_current_frame_begin();
Descriptor *swapchain_current_frame_global_descriptor();
void *swapchain_current_frame_get_buffer(frame_buffer_type_t buffer_type);
void swapchain_current_frame_unmap_buffers();

VkCommandBuffer swapchain_current_frame_command_buffer();
Image *swapchain_current_image();

void swapchain_descriptors_write_texture(Image *image, uint32_t arr_index, VkSampler sampler);
