#include "renderer.h"

#include "draw.h"
#include "gpu_model.h"
#include "passes/passes.h"
#include "platform.h"
#include "render_graph.h"
#include "sampler.h"
#include "swapchain.h"
#include "vk_context.h"
#include "world.h"

#include "buffer.h"
#include "command.h"
#include "descriptors.h"
#include "image.h"
#include "pipeline.h"

#include "common/array.h"
#include "common/util.h"

#include "husky.h"

#include <cglm/cam.h>

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

DescriptorLayout g_GlobalDescriptorLayout;
VkDescriptorSetLayout g_MaterialDescriptorLayout;

GraphicsPipeline *g_ActiveGraphicsPipeline;

render_graph_t g_render_graph;

bool RendererInit(RendererCreateInfo *c) {
        platform_init(c->width, c->height, c->title);
        vk_context_init();

        vk_memory_allocator_init();

        descriptors_init();
        samplers_init();

        ASSERT(swapchain_create());

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

void RendererShutdown() {
        // Make sure the GPU has finished all work
        vk_context_wait_idle();

        gpu_unload_models();
        draw_buffers_shutdown();

        swapchain_destroy();
        immediate_command_shutdown();

        samplers_shutdown();
        descriptor_allocator_destroy();
        descriptor_layout_destroy(&g_GlobalDescriptorLayout);
        // vkDestroyDescriptorSetLayout(vk_context_device(), g_MaterialDescriptorLayout, NULL);

        gradient_pass_cleanup();
        pbr_pass_cleanup();
        render_graph_destroy(&g_render_graph);

        vk_memory_allocator_shutdown();

        vk_context_shutdown();
        platform_shutdown();
}

void renderer_draw() {

        render_graph_execute(&g_render_graph, swapchain_current_frame_command_buffer());
}

void DrawCommandSetSceneData(SceneData *camera) {
        SceneData *scene_data = swapchain_current_frame_get_buffer(FRAME_BUFFER_CAMERA);
        *scene_data = *camera;
        swapchain_current_frame_unmap_buffer(FRAME_BUFFER_CAMERA);
}

bool DrawCommandBeginFrame() {
        if (!swapchain_next_frame()) {
                SwapchainRecreate();
        }

        swapchain_current_frame_begin();

        return true;
}

void DrawCommandEndFrame() {
        image_transition(swapchain_current_image(), swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(swapchain_current_frame_command_buffer());

        swapchain_current_frame_submit();

        platform_update_window();
}

void agpu_begin_frame() {
        if (platform_size_changed()) {
                SwapchainRecreate();
        }

        if (!DrawCommandBeginFrame()) {
                return;
        }
}

void agpu_end_frame() { DrawCommandEndFrame(); }

void agpu_set_camera(Camera camera) {
        SceneData scene = {0};

        // Vulkan internally uses an inverted Y-axis compared to cglm. That is, y=0 is the top of
        // the screen, while y=MAX is the bottom. To adjust for this we need to invert the view
        // matrix along the y axis. Now, we also need to flip the position of the camera eye on the
        // y-axis or vertical movement will be inverted

        vec3 eye = {camera.position[0], -1 * camera.position[1], camera.position[2]};
        vec3 flip = {1.0f, -1.0f, 1.0f};

        glm_look(eye, camera.target, camera.up, scene.view);
        glm_scale(scene.view, flip);

        uint32_t w, h;
        platform_get_size(&w, &h);

        glm_perspective(glm_rad(camera.fov), (float)w / h, 0.1f, 100.0f, scene.proj);

        glm_mat4_mul(scene.proj, scene.view, scene.viewproj);

        scene.ambientColor[0] = 1.0f;
        scene.ambientColor[1] = 1.0f;
        scene.ambientColor[2] = 1.0f;
        scene.ambientColor[3] = 0.0f;

        scene.sunlightDirection[0] = 0.3f;
        scene.sunlightDirection[1] = 1.0f;
        scene.sunlightDirection[2] = 0.3f;
        scene.sunlightDirection[3] = 0.1f;
        DrawCommandSetSceneData(&scene);
}
