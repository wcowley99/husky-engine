#include "renderer/renderer.h"

#include "renderer/buffer.h"
#include "renderer/command.h"
#include "renderer/descriptors.h"
#include "renderer/draw.h"
#include "renderer/gpu_model.h"
#include "renderer/image.h"
#include "renderer/platform.h"
#include "renderer/render_graph.h"
#include "renderer/render_passes.h"
#include "renderer/sampler.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "husky.h"

static render_graph_t g_render_graph;

bool renderer_init(renderer_config_t *c) {
        platform_init(c->width, c->height, c->title);
        vk_context_init();

        vk_memory_allocator_init();

        descriptors_init();
        samplers_init();

        swapchain_create();

        immediate_command_init();

        attachment_handle_t hdr = render_graph_add_attachment(
            &g_render_graph, (VkExtent3D){c->width, c->height, 1}, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

        attachment_handle_t depth = render_graph_add_attachment(
            &g_render_graph, (VkExtent3D){c->width, c->height, 1}, VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        gradient_pass_register(&g_render_graph, hdr);
        pbr_pass_register(&g_render_graph, hdr, depth);
        present_pass_register(&g_render_graph, hdr);

        draw_buffers_init();

        return true;
}

void renderer_shutdown() {
        // Make sure the GPU has finished all work
        vk_context_wait_idle();

        gpu_unload_models();
        draw_buffers_shutdown();

        swapchain_destroy();
        immediate_command_shutdown();

        samplers_shutdown();
        descriptors_shutdown();

        render_graph_destroy(&g_render_graph);

        vk_memory_allocator_shutdown();

        vk_context_shutdown();
        platform_shutdown();
}

void renderer_draw() {
        draw_batches_upload();

        render_graph_execute(&g_render_graph, swapchain_current_frame_command_buffer());
}

void renderer_end_frame() {
        image_transition(swapchain_current_image(), swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(swapchain_current_frame_command_buffer());

        swapchain_current_frame_submit();

        platform_update_window();
}

void renderer_begin_frame() {
        if (platform_size_changed()) {
                SwapchainRecreate();
        }

        if (!swapchain_next_frame()) {
                SwapchainRecreate();
        }

        swapchain_current_frame_begin();

        draw_buffers_clear();
}
