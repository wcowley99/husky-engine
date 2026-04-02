#pragma once

#include "descriptors.h"
#include "image.h"
#include "vkb.h"

#include <stdint.h>

#define MAX_ATTACHMENTS 32
#define MAX_PASSES 32

typedef uint32_t attachment_handle_t;
#define ATTACHMENT_BACKBUFFER UINT32_MAX // swapchain image

typedef struct render_attachment {
        AllocatedImage image;
} render_attachment_t;

typedef struct render_pass {
        void (*record)(VkCommandBuffer);

        attachment_handle_t attachments[8];
        VkImageLayout attachment_states[8];
        uint32_t attachment_count;
} render_pass_t;

typedef struct render_graph {
        render_attachment_t attachments[MAX_ATTACHMENTS];
        uint32_t attachment_count;

        render_pass_t render_passes[MAX_PASSES];
        uint32_t pass_count;
} render_graph_t;

attachment_handle_t render_graph_add_attachment(render_graph_t *graph, VkExtent3D extent,
                                                VkFormat format, VkImageAspectFlags flags,
                                                VkImageUsageFlags usage);

void render_graph_destroy(render_graph_t *graph);

VkFormat render_graph_attachment_format(render_graph_t *graph, attachment_handle_t attachment_ref);
Image *render_graph_attachment_image(render_graph_t *graph, attachment_handle_t attachment_ref);

void render_graph_register_pass(render_graph_t *graph, render_pass_t pass);

void render_graph_execute(render_graph_t *graph, VkCommandBuffer cmd);
