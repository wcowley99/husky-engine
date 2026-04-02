#include "passes.h"

#include "renderer/descriptors.h"
#include "renderer/draw.h"
#include "renderer/pipeline.h"
#include "renderer/render_graph.h"
#include "renderer/renderer.h"
#include "renderer/swapchain.h"
#include "renderer/vk_context.h"

#include "common/array.h"
#include "common/util.h"

typedef struct pbr_pass {
        GraphicsPipeline pipeline;
        attachment_handle_t hdr;
        attachment_handle_t depth;
} pbr_pass_t;

static pbr_pass_t g_pbr_pass;

static void pbr_pipeline_init(VkFormat format) {
        VkPushConstantRange push_constants[] = {
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = 72,
            },
        };

        size_t vert_size, frag_size;
        char *vert = ReadFile("shaders/pbr.vert.spv", &vert_size);
        char *frag = ReadFile("shaders/pbr.frag.spv", &frag_size);

        VkDescriptorSetLayout layouts[] = {global_descriptor_layout()->layout};
        GraphicsPipelineCreateInfo mesh_pipeline_info = {
            .descriptors = layouts,
            .num_descriptors = 1,
            .push_constants = push_constants,
            .num_push_constants = 1,
            .vertex_shader = (const uint32_t *)vert,
            .vertex_shader_size = vert_size / 4,
            .fragment_shader = (const uint32_t *)frag,
            .fragment_shader_size = frag_size / 4,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .polygon_mode = VK_POLYGON_MODE_FILL,
            .cull_mode = VK_CULL_MODE_BACK_BIT,
            .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .color_attachment_format = format,
            .depth_attachment_format = VK_FORMAT_D32_SFLOAT,
            .depth_testing = true,
            .depth_compare_op = VK_COMPARE_OP_LESS,
        };
        graphics_pipeline_create(vk_context_device(), &mesh_pipeline_info, &g_pbr_pass.pipeline);

        free(vert);
        free(frag);
}

void pbr_callback(VkCommandBuffer cmd) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pbr_pass.pipeline.layout, 0,
                                1, &swapchain_current_frame_global_descriptor()->descriptor, 0,
                                NULL);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pbr_pass.pipeline.pipeline);

        draw_batches_record();
}

void pbr_pass_register(render_graph_t *graph, attachment_handle_t hdr, attachment_handle_t depth) {
        g_pbr_pass.hdr = hdr;
        g_pbr_pass.depth = depth;

        pbr_pipeline_init(render_graph_attachment_format(graph, hdr));

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
