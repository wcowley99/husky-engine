#include "renderer/render_passes.h"

#include "renderer/render_graph.h"
#include "renderer/swapchain.h"

typedef struct present_pass_t {
        render_graph_t *graph;
        attachment_handle_t image;
} present_pass_t;

static present_pass_t g_present_pass;

static void present_callback(VkCommandBuffer cmd) {
        Image *src = render_graph_attachment_image(g_present_pass.graph, g_present_pass.image);
        Image *dst = render_graph_attachment_image(g_present_pass.graph, ATTACHMENT_BACKBUFFER);

        image_blit(swapchain_current_frame_command_buffer(), src, dst);
}

void present_pass_register(render_graph_t *graph, attachment_handle_t image) {
        g_present_pass.image = image;
        g_present_pass.graph = graph;

        render_pass_t pass = {
            .record = present_callback,
            .attachment_count = 2,
            .attachments = {image, ATTACHMENT_BACKBUFFER},
            .attachment_states = {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
        };

        render_graph_register_pass(graph, pass);
}
