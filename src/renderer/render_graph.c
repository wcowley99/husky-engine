#include "renderer/render_graph.h"

#include "renderer/image.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "husky.h"

attachment_handle_t render_graph_add_attachment(render_graph_t *graph, VkExtent3D extent,
                                                VkFormat format, VkImageAspectFlags flags,
                                                VkImageUsageFlags usage) {
        ASSERT(graph->attachment_count < MAX_ATTACHMENTS);

        attachment_handle_t attachment_ref = (attachment_handle_t)graph->attachment_count;
        graph->attachment_count++;

        AllocatedImageCreateInfo allocated_image_info = {
            .extent = extent,
            .format = format,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .aspect_flags = flags,
            .usage_flags = usage,
        };

        allocated_image_create(&allocated_image_info, &graph->attachments[attachment_ref].image);

        return attachment_ref;
}

void render_graph_destroy(render_graph_t *graph) {
        for (int i = 0; i < graph->attachment_count; i++) {
                allocated_image_destroy(&graph->attachments[i].image, vk_context_device());
        }

        for (int i = 0; i < graph->pass_count; i++) {
                render_pass_t *pass = &graph->render_passes[i];

                if (pass->cleanup)
                        pass->cleanup();
        }
}

VkFormat render_graph_attachment_format(render_graph_t *graph, attachment_handle_t attachment_ref) {
        if (attachment_ref == UINT32_MAX) {
                return swapchain_current_image()->format;
        }

        ASSERT(attachment_ref < graph->attachment_count);

        return graph->attachments[attachment_ref].image.image.format;
}

Image *render_graph_attachment_image(render_graph_t *graph, attachment_handle_t attachment_ref) {
        if (attachment_ref == UINT32_MAX) {
                return swapchain_current_image();
        }

        ASSERT(attachment_ref < graph->attachment_count);

        return &graph->attachments[attachment_ref].image.image;
}

void render_graph_register_pass(render_graph_t *graph, render_pass_t pass) {
        ASSERT(graph->pass_count < MAX_PASSES);

        graph->render_passes[graph->pass_count] = pass;
        graph->pass_count++;
}

static void begin_rendering(VkCommandBuffer cmd, VkRenderingAttachmentInfo *colors,
                            uint32_t color_count, VkRenderingAttachmentInfo *depth, uint32_t width,
                            uint32_t height) {
        VkRect2D scissor = {
            {0, 0},
            {.width = width, .height = height},
        };
        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = scissor,
            .colorAttachmentCount = color_count,
            .pColorAttachments = colors,
            .pDepthAttachment = depth,
            .layerCount = 1,
        };

        vkCmdBeginRendering(cmd, &render_info);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = width,
            .height = height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
}

VkRenderingAttachmentInfo color_attachment(Image *image) {
        return (VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = image->image_view,
            .imageLayout = image->layout,
        };
}

static VkRenderingAttachmentInfo depth_attachment(Image *image) {
        return (VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = image->image_view,
            .imageLayout = image->layout,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue.depthStencil.depth = 1.0f,
        };
}

void render_graph_execute(render_graph_t *graph, VkCommandBuffer cmd) {
        for (int i = 0; i < graph->pass_count; i++) {
                render_pass_t *pass = &graph->render_passes[i];

                VkRenderingAttachmentInfo color_attachments[8];
                uint32_t color_attachments_count = 0;

                VkRenderingAttachmentInfo depth;
                bool has_depth = false;

                uint32_t width = 0, height = 0;

                for (int a = 0; a < pass->attachment_count; a++) {
                        attachment_handle_t attachment_ref = pass->attachments[a];
                        Image *attachment = render_graph_attachment_image(graph, attachment_ref);
                        image_transition(attachment, cmd, pass->attachment_states[a]);

                        if (pass->attachment_states[a] ==
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                                color_attachments[color_attachments_count] =
                                    color_attachment(attachment);
                                color_attachments_count++;
                        }
                        if (pass->attachment_states[a] ==
                            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
                                depth = depth_attachment(attachment);
                                has_depth = true;
                        }

                        width = attachment->extent.width;
                        height = attachment->extent.height;
                }

                if (color_attachments_count > 0 || has_depth) {
                        VkRenderingAttachmentInfo *depth_ref = has_depth ? &depth : VK_NULL_HANDLE;
                        begin_rendering(cmd, color_attachments, color_attachments_count, depth_ref,
                                        width, height);
                }

                graph->render_passes[i].record(cmd);

                if (color_attachments_count > 0 || has_depth) {
                        vkCmdEndRendering(cmd);
                }
        }
}
