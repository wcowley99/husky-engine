#include "renderer/render_passes.h"

#include "renderer/descriptors.h"
#include "renderer/pipeline.h"
#include "renderer/vk_context.h"

#include "common/util.h"

#include "husky.h"

typedef struct gradient_pass {
        ComputePipeline pipeline;
        DescriptorLayout pass_layout;
        Descriptor pass_descriptor;

        render_graph_t *graph;
        attachment_handle_t image_ref;
} gradient_pass_t;

static gradient_pass_t g_gradient_pass;

static void gradient_callback(VkCommandBuffer cmd);
static void gradient_pass_cleanup();

void gradient_pass_register(render_graph_t *graph, attachment_handle_t image) {
        g_gradient_pass.image_ref = image;
        g_gradient_pass.graph = graph;

        DescriptorBinding draw_image_bindings[1] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .count = 1,
             .stage = VK_SHADER_STAGE_COMPUTE_BIT},
        };

        descriptor_allocator_reserve(draw_image_bindings, 1, false);
        g_gradient_pass.pass_layout =
            descriptor_layout_create(vk_context_device(), draw_image_bindings, 1);

        g_gradient_pass.pass_descriptor = descriptor_allocate(&g_gradient_pass.pass_layout);

        descriptor_write_image(g_gradient_pass.pass_descriptor,
                               render_graph_attachment_image(graph, image), 0, 0);

        size_t gradient_size;
        char *gradient_comp = ReadFile("shaders/gradient2.comp.spv", &gradient_size);

        uint32_t sizes[] = {sizeof(float) * 16};
        ComputePipelineInfo pipeline_info = {
            .descriptors = &g_gradient_pass.pass_layout.layout,
            .num_descriptors = 1,
            .push_constant_sizes = sizes,
            .num_push_constant_sizes = 1,
            .shader_source = (const uint32_t *)gradient_comp,
            .shader_source_size = gradient_size / 4,
        };
        ASSERT(compute_pipeline_create(vk_context_device(), &pipeline_info,
                                       &g_gradient_pass.pipeline));
        free(gradient_comp);

        render_pass_t pass = {
            .record = gradient_callback,
            .cleanup = gradient_pass_cleanup,
            .attachment_count = 1,
            .attachments = {image},
            .attachment_states = {VK_IMAGE_LAYOUT_GENERAL},
        };

        render_graph_register_pass(graph, pass);
}

static void gradient_callback(VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, g_gradient_pass.pipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                g_gradient_pass.pipeline.layout, 0, 1,
                                &g_gradient_pass.pass_descriptor.descriptor, 0, NULL);

        struct {
                vec4 top;
                vec4 bottom;
                float padding[8];
        } pc = {
            .top = {1.0f, 0.0f, 0.0f, 1.0f},
            .bottom = {0.0f, 0.0f, 1.0f, 1.0f},
        };

        vkCmdPushConstants(cmd, g_gradient_pass.pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(pc), &pc);

        Image *image =
            render_graph_attachment_image(g_gradient_pass.graph, g_gradient_pass.image_ref);
        vkCmdDispatch(cmd, ceilf(image->extent.width / 16.0f), ceilf(image->extent.height / 16.0f),
                      1);
}

static void gradient_pass_cleanup() {
        compute_pipeline_destroy(&g_gradient_pass.pipeline, vk_context_device());
        descriptor_layout_destroy(&g_gradient_pass.pass_layout);
}
