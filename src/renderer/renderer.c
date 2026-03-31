#include "renderer.h"

#include "platform.h"
#include "swapchain.h"
#include "vk_context.h"

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

ImmediateCommand g_ImmediateCommand;

DescriptorLayout g_GlobalDescriptorLayout;
VkDescriptorSetLayout g_MaterialDescriptorLayout;

AllocatedImage g_IntermediateImage;
AllocatedImage g_DepthImage;
DescriptorLayout g_IntermediateImageDescriptorLayout;
Descriptor g_IntermediateImageDescriptors;

ComputePipeline g_GradientPipeline;
GraphicsPipeline g_PbrPipeline;

GraphicsPipeline *g_ActiveGraphicsPipeline;

MeshBuffer *g_MeshBuffers;
AllocatedImage *g_Textures;

RenderObject *g_RenderObjects;

Material g_DefaultMaterial;

VkSampler g_LinearSampler;
VkSampler g_NearestSampler;

uint32_t mesh_buffer_create(Mesh *mesh) {
        const size_t vertex_buffer_size = sizeof(Vertex) * array_length(mesh->vertices);
        const size_t index_buffer_size = sizeof(uint32_t) * array_length(mesh->indices);

        uint32_t index = array_length(g_MeshBuffers);
        array_append(g_MeshBuffers, (MeshBuffer){0});
        MeshBuffer *buffer = &g_MeshBuffers[index];
        buffer->num_indices = array_length(mesh->indices);

        buffer_create(vertex_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->vertex);
        buffer_create(index_buffer_size,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->index);

        VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer->vertex.buffer,
        };
        buffer->vertex_address = vkGetBufferDeviceAddress(vk_context_device(), &address_info);

        Buffer staging_buffer;
        buffer_create(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, &staging_buffer);
        vmaCopyMemoryToAllocation(staging_buffer.allocator, mesh->vertices,
                                  staging_buffer.allocation, 0, vertex_buffer_size);
        vmaCopyMemoryToAllocation(staging_buffer.allocator, mesh->indices,
                                  staging_buffer.allocation, vertex_buffer_size, index_buffer_size);

        VkCommandBuffer cmd = g_ImmediateCommand.command;
        immediate_command_begin(&g_ImmediateCommand);

        VkBufferCopy vertex_copy = {.size = vertex_buffer_size};
        VkBufferCopy index_copy = {.size = index_buffer_size, .srcOffset = vertex_buffer_size};

        vkCmdCopyBuffer(cmd, staging_buffer.buffer, buffer->vertex.buffer, 1, &vertex_copy);
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, buffer->index.buffer, 1, &index_copy);

        immediate_command_end(&g_ImmediateCommand);

        buffer_destroy(&staging_buffer);
        return index;
}

void mesh_buffer_destroy(MeshBuffer *buffer) {
        buffer_destroy(&buffer->vertex);
        buffer_destroy(&buffer->index);
}

///////////////////////////////////////
/// Renderer
///////////////////////////////////////
bool RendererInit(RendererCreateInfo *c) {
        platform_init(c->width, c->height, c->title);
        vk_context_init();

        vk_memory_allocator_init();

        AllocatedImageCreateInfo allocated_image_info = {
            .extent =
                {
                    .width = c->width,
                    .height = c->height,
                    .depth = 1,
                },
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            .usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .allocator = vk_memory_allocator(),
            .device = vk_context_device(),
        };

        AllocatedImageCreateInfo depth_image_info = {
            .extent =
                {
                    .width = c->width,
                    .height = c->height,
                    .depth = 1,
                },
            .format = VK_FORMAT_D32_SFLOAT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT,
            .usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .allocator = vk_memory_allocator(),
            .device = vk_context_device(),
        };

        ASSERT(allocated_image_create(&allocated_image_info, &g_IntermediateImage));
        ASSERT(allocated_image_create(&depth_image_info, &g_DepthImage));

        create_samplers();

        ASSERT(init_descriptors());

        ASSERT(swapchain_create());

        ASSERT(immediate_command_create(vk_context_device(), vk_context_queue_family_index(),
                                        vk_context_graphics_queue(), &g_ImmediateCommand));

        size_t gradient_size;
        char *gradient_comp = ReadFile("shaders/gradient2.comp.spv", &gradient_size);

        uint32_t sizes[] = {sizeof(float) * 16};
        ComputePipelineInfo pipeline_info = {
            .descriptors = &g_IntermediateImageDescriptorLayout.layout,
            .num_descriptors = 1,
            .push_constant_sizes = sizes,
            .num_push_constant_sizes = 1,
            .shader_source = (const uint32_t *)gradient_comp,
            .shader_source_size = gradient_size / 4,
        };
        ASSERT(compute_pipeline_create(vk_context_device(), &pipeline_info, &g_GradientPipeline));

        free(gradient_comp);

        g_PbrPipeline =
            create_pbr_pipeline(vk_context_device(), g_GlobalDescriptorLayout.layout,
                                g_MaterialDescriptorLayout, g_IntermediateImage.image.format);

        g_MeshBuffers = array(MeshBuffer);
        g_Textures = array(AllocatedImage);

        g_RenderObjects = array(RenderObject);

        return true;
}

void RendererShutdown() {
        // Make sure the GPU has finished all work
        vk_context_wait_idle();

        compute_pipeline_destroy(&g_GradientPipeline, vk_context_device());
        graphics_pipeline_destroy(&g_PbrPipeline, vk_context_device());

        vkDestroySampler(vk_context_device(), g_LinearSampler, NULL);
        vkDestroySampler(vk_context_device(), g_NearestSampler, NULL);

        allocated_image_destroy(&g_IntermediateImage, vk_context_device());
        allocated_image_destroy(&g_DepthImage, vk_context_device());

        for (int i = 0; i < array_length(g_MeshBuffers); i += 1) {
                mesh_buffer_destroy(&g_MeshBuffers[i]);
        }
        array_free(g_MeshBuffers);

        for (int i = 0; i < array_length(g_Textures); i += 1) {
                allocated_image_destroy(&g_Textures[i], vk_context_device());
        }
        array_free(g_Textures);
        array_free(g_RenderObjects);

        swapchain_destroy();
        immediate_command_destroy(&g_ImmediateCommand);

        descriptor_allocator_destroy();
        descriptor_layout_destroy(&g_GlobalDescriptorLayout);
        descriptor_layout_destroy(&g_IntermediateImageDescriptorLayout);
        // vkDestroyDescriptorSetLayout(vk_context_device(), g_MaterialDescriptorLayout, NULL);

        vk_memory_allocator_shutdown();

        vk_context_shutdown();
        platform_shutdown();
}

void DrawCommandClear(Image *image, VkClearColorValue color) {
        image_transition(image, swapchain_current_frame_command_buffer(), VK_IMAGE_LAYOUT_GENERAL);
        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };
        vkCmdClearColorImage(swapchain_current_frame_command_buffer(), image->image, image->layout,
                             &color, 1, &range);
}

void DrawCommandBindCompute(ComputePipeline *compute) {
        vkCmdBindPipeline(swapchain_current_frame_command_buffer(), VK_PIPELINE_BIND_POINT_COMPUTE,
                          compute->pipeline);
}

void DrawCommandSetComputePushConstants(ComputePipeline *compute, size_t offset, size_t size,
                                        void *data) {
        vkCmdPushConstants(swapchain_current_frame_command_buffer(), g_GradientPipeline.layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, offset, size, data);
}

void DrawCommandBindComputeDescriptor(ComputePipeline *compute, VkDescriptorSet descriptor,
                                      uint32_t location) {
        vkCmdBindDescriptorSets(swapchain_current_frame_command_buffer(),
                                VK_PIPELINE_BIND_POINT_COMPUTE, compute->layout, location, 1,
                                &descriptor, 0, NULL);
}

void DrawCommandExecuteCompute(Image *image) {
        vkCmdDispatch(swapchain_current_frame_command_buffer(), ceilf(image->extent.width / 16.0f),
                      ceilf(image->extent.height / 16.0f), 1);
}

void DrawCommandBeginRendering(Image *image) {
        VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = image->image_view,
            .imageLayout = image->layout,
        };
        VkRenderingAttachmentInfo depth_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = g_DepthImage.image.image_view,
            .imageLayout = g_DepthImage.image.layout,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue.depthStencil.depth = 1.0f,
        };
        VkRect2D scissor = {
            {0, 0},
            {.width = image->extent.width, .height = image->extent.height},
        };
        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = scissor,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
            .pDepthAttachment = &depth_attachment,
            .layerCount = 1,
        };

        vkCmdBeginRendering(swapchain_current_frame_command_buffer(), &render_info);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = image->extent.width,
            .height = image->extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(swapchain_current_frame_command_buffer(), 0, 1, &viewport);
        vkCmdSetScissor(swapchain_current_frame_command_buffer(), 0, 1, &scissor);
}

void DrawCommandBindGraphicsPipeline(GraphicsPipeline *graphics) {
        vkCmdBindPipeline(swapchain_current_frame_command_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics->pipeline);
        g_ActiveGraphicsPipeline = graphics;
}

void DrawCommandSetSceneData(SceneData *camera) {
        if (!g_ActiveGraphicsPipeline) {
                printf("No currently bound graphics pipeline!");
                exit(1);
        }

        SceneData *scene_data = swapchain_current_frame_get_buffer(FRAME_BUFFER_CAMERA);
        *scene_data = *camera;

        vkCmdBindDescriptorSets(swapchain_current_frame_command_buffer(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS, g_ActiveGraphicsPipeline->layout,
                                0, 1, &swapchain_current_frame_global_descriptor()->descriptor, 0,
                                NULL);
}

void DrawCommandSetGraphicsPushConstants(size_t offset, size_t size, void *data) {
        vkCmdPushConstants(swapchain_current_frame_command_buffer(),
                           g_ActiveGraphicsPipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, offset,
                           size, data);
}

void DrawCommandBindIndexBuffer(const MeshBuffer *mesh) {
        vkCmdBindIndexBuffer(swapchain_current_frame_command_buffer(), mesh->index.buffer, 0,
                             VK_INDEX_TYPE_UINT32);
}

void DrawCommandCopyToSwapchain(Image *image) {
        image_transition(image, swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        image_transition(swapchain_current_image(), swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        image_blit(swapchain_current_frame_command_buffer(), image, swapchain_current_image());
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

uint32_t gpu_upload_texture(MaterialInfo *mats) {
        AllocatedImageCreateInfo create_info = {
            .extent = (VkExtent3D){mats->diffuse_width, mats->diffuse_height, 1},
            .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,

            .device = vk_context_device(),
            .allocator = vk_memory_allocator(),

            .data = (uint32_t *)mats->diffuse_tex,
            .imm = &g_ImmediateCommand,
        };

        uint32_t index = array_length(g_Textures);
        array_append(g_Textures, (AllocatedImage){0});
        AllocatedImage *image = &g_Textures[index];
        allocated_image_create(&create_info, image);

        swapchain_descriptors_write_texture(&g_Textures[index].image, index, g_LinearSampler);

        return index;
}

GpuModel agpu_load_model(char *filename) {
        Model m = load_model(filename);
        GpuModel r;
        r.meshes = array(GpuMesh);

        printf("#meshes = %zu, #materials = %zu\n", array_length(m.meshes),
               array_length(m.materials));

        for (int i = 0; i < array_length(m.meshes); i++) {
                Mesh *cpu_mesh = &m.meshes[i];
                MaterialInfo *material = &m.materials[cpu_mesh->material_index];
                GpuMesh mesh = {
                    .mesh = mesh_buffer_create(cpu_mesh),
                    .material = &g_DefaultMaterial,
                    .diffuse_tex = gpu_upload_texture(material),
                };

                array_append(r.meshes, mesh);
        }

        model_destroy(m);

        return r;
}

bool init_descriptors() {
        descriptor_allocator_init(NUM_FRAMES);

        DescriptorBinding draw_image_bindings[1] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .count = 1,
             .stage = VK_SHADER_STAGE_COMPUTE_BIT},
        };

        descriptor_allocator_reserve(draw_image_bindings, 1, false);
        g_IntermediateImageDescriptorLayout =
            descriptor_layout_create(vk_context_device(), draw_image_bindings, 1);

        global_descriptor_layout_init();

        ASSERT(descriptor_allocator_create(vk_context_device()));
        g_IntermediateImageDescriptors = descriptor_allocate(&g_IntermediateImageDescriptorLayout);

        descriptor_write_image(g_IntermediateImageDescriptors, &g_IntermediateImage.image, 0, 0);

        return true;
}

void create_samplers() {
        VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;

        vkCreateSampler(vk_context_device(), &sampler_info, NULL, &g_LinearSampler);

        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;

        vkCreateSampler(vk_context_device(), &sampler_info, NULL, &g_NearestSampler);
}

DrawBatch draw_batch_create(RenderObject obj, uint32_t first_instance) {
        DrawBatch batch = {0};
        batch.object = obj;
        batch.first_instance = first_instance;
        batch.count = 0;

        return batch;
}

void draw_batch_add(DrawBatch *batch, RenderObject obj, Instance *ssbo) {
        uint32_t index = batch->first_instance + batch->count;
        batch->count += 1;
        const MeshBuffer *mesh = &g_MeshBuffers[batch->object.mesh];

        ssbo[index] = (Instance){
            .vertex_address = mesh->vertex_address,
            .tex_index = obj.tex_index,
        };
        glm_mat4_dup(obj.transform, ssbo[index].model);
}

void draw_batch_draw(DrawBatch *batch) {
        const MeshBuffer *mesh = &g_MeshBuffers[batch->object.mesh];
        DrawCommandBindIndexBuffer(mesh);
        vkCmdDrawIndexed(swapchain_current_frame_command_buffer(), mesh->num_indices, batch->count,
                         0, 0, batch->first_instance);
}

void agpu_begin_frame() {
        if (platform_size_changed()) {
                SwapchainRecreate();
        }

        if (!DrawCommandBeginFrame()) {
                return;
        }
        DrawCommandClear(&g_IntermediateImage.image, (VkClearColorValue){0.0f, 0.0f, 1.0f, 0.0f});

        array_clear(g_RenderObjects);

        // compute pipeline
        image_transition(&g_IntermediateImage.image, swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_GENERAL);
        DrawCommandBindCompute(&g_GradientPipeline);
        DrawCommandBindComputeDescriptor(&g_GradientPipeline,
                                         g_IntermediateImageDescriptors.descriptor, 0);

        struct {
                vec4 top;
                vec4 bottom;
                float padding[8];
        } pc = {
            .top = {1.0f, 0.0f, 0.0f, 1.0f},
            .bottom = {0.0f, 0.0f, 1.0f, 1.0f},
        };
        DrawCommandSetComputePushConstants(&g_GradientPipeline, 0, sizeof(pc), &pc);
        DrawCommandExecuteCompute(&g_IntermediateImage.image);

        // graphics pipeline
        image_transition(&g_IntermediateImage.image, swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        image_transition(&g_DepthImage.image, swapchain_current_frame_command_buffer(),
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        DrawCommandBeginRendering(&g_IntermediateImage.image);
}

void agpu_end_frame() {
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

        vkCmdEndRendering(swapchain_current_frame_command_buffer());

        DrawCommandCopyToSwapchain(&g_IntermediateImage.image);
        DrawCommandEndFrame();
}

void agpu_set_camera(Camera camera) {
        DrawCommandBindGraphicsPipeline(&g_PbrPipeline);

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

void agpu_draw_model(GpuModel model, mat4 transform) {
        for (int i = 0; i < array_length(model.meshes); i++) {
                GpuMesh mesh = model.meshes[i];
                RenderObject obj = {
                    .material = mesh.material,
                    .mesh = mesh.mesh,
                    .tex_index = mesh.diffuse_tex,
                };
                glm_mat4_dup(transform, obj.transform);

                array_append(g_RenderObjects, obj);
        }
}

bool render_object_compatible(RenderObject a, RenderObject b) {
        return (a.mesh == b.mesh) && (a.material == b.material);
}
