#include "passes.h"

#include "renderer/descriptors.h"
#include "renderer/pipeline.h"
#include "renderer/render_graph.h"
#include "renderer/renderer.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "common/array.h"

typedef struct pbr_pass {
        GraphicsPipeline pipeline;
        attachment_handle_t hdr;
        attachment_handle_t depth;
} pbr_pass_t;

static pbr_pass_t g_pbr_pass;

void pbr_callback(VkCommandBuffer cmd) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pbr_pass.pipeline.layout, 0,
                                1, &swapchain_current_frame_global_descriptor()->descriptor, 0,
                                NULL);

        vkCmdBindPipeline(swapchain_current_frame_command_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g_pbr_pass.pipeline.pipeline);

        Instance *ssbo = (Instance *)swapchain_current_frame_get_buffer(FRAME_BUFFER_INSTANCES);

        uint32_t instance_count = 0;

        DrawBatch batch = draw_batch_create(g_RenderObjects[0], instance_count);
        draw_batch_add(&batch, g_RenderObjects[0], ssbo);
        instance_count++;

        for (int i = 1; i < array_length(g_RenderObjects); i += 1) {
                if (!render_object_compatible(batch.object, g_RenderObjects[i])) {
                        draw_batch_draw(&batch);

                        batch = draw_batch_create(g_RenderObjects[i], instance_count);
                        draw_batch_add(&batch, g_RenderObjects[i], ssbo);
                } else {
                        draw_batch_add(&batch, g_RenderObjects[i], ssbo);
                }

                instance_count++;
        }

        draw_batch_draw(&batch);

        swapchain_current_frame_unmap_buffers();
}

void pbr_pass_register(render_graph_t *graph, attachment_handle_t hdr, attachment_handle_t depth) {
        g_pbr_pass.hdr = hdr;
        g_pbr_pass.depth = depth;

        g_pbr_pass.pipeline =
            create_pbr_pipeline(vk_context_device(), render_graph_attachment_format(graph, hdr));

        render_pass_t pass = {
            .record = pbr_callback,
            .attachment_count = 2,
            .attachments = {hdr, depth},
            .attachment_states = {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL},
        };

        render_graph_register_pass(graph, pass);
}

void pbr_pass_cleanup() { graphics_pipeline_destroy(&g_pbr_pass.pipeline, vk_context_device()); }
