#include "renderer.h"

#include "platform.h"
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

// Swapchain
VkSwapchainKHR g_Swapchain;
uint32_t g_SwapchainImageCount;
uint32_t g_SwapchainFrameIndex;
Image *g_SwapchainImages;
FrameResources *g_SwapchainFrameResources;
const VkFormat g_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
const uint32_t NUM_FRAMES = 3;

// A reference to the current swapchain image
Image *g_CurrentSwapchainImage;

// A reference to the resources of the current swapchain frame
FrameResources *g_CurrentFrameResources;

// The current swapchain image index
uint32_t g_CurrentSwapchainImageIndex;

VmaAllocator g_Allocator;
ImmediateCommand g_ImmediateCommand;

DescriptorAllocator g_DescriptorAllocator;

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

        buffer_create(g_Allocator, vertex_buffer_size,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->vertex);
        buffer_create(g_Allocator, index_buffer_size,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, &buffer->index);

        VkBufferDeviceAddressInfo address_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer->vertex.buffer,
        };
        buffer->vertex_address = vkGetBufferDeviceAddress(vk_context_device(), &address_info);

        Buffer staging_buffer;
        buffer_create(g_Allocator, vertex_buffer_size + index_buffer_size,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, &staging_buffer);
        vmaCopyMemoryToAllocation(g_Allocator, mesh->vertices, staging_buffer.allocation, 0,
                                  vertex_buffer_size);
        vmaCopyMemoryToAllocation(g_Allocator, mesh->indices, staging_buffer.allocation,
                                  vertex_buffer_size, index_buffer_size);

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
/// FrameResources
///////////////////////////////////////

bool frame_resources_create(FrameResources *f) {
        VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = vk_context_queue_family_index(),
        };
        VK_EXPECT(vkCreateCommandPool(vk_context_device(), &command_pool_info, NULL, &f->pool));

        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = f->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_EXPECT(vkAllocateCommandBuffers(vk_context_device(), &alloc_info, &f->command));

        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_EXPECT(vkCreateFence(vk_context_device(), &fence_info, NULL, &f->render_fence));

        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = 0,
        };
        VK_EXPECT(
            vkCreateSemaphore(vk_context_device(), &semaphore_info, NULL, &f->swapchain_semaphore));
        VK_EXPECT(
            vkCreateSemaphore(vk_context_device(), &semaphore_info, NULL, &f->render_semaphore));

        ASSERT(buffer_create(g_Allocator, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU, &f->camera_uniform));
        ASSERT(buffer_create(g_Allocator, sizeof(Instance) * MAX_INSTANCES,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, &f->instance_buffer));

        f->global_descriptors =
            descriptor_allocate(&g_DescriptorAllocator, &g_GlobalDescriptorLayout);

        descriptor_write_buffer(f->global_descriptors, &f->camera_uniform, 0, 0);
        descriptor_write_buffer(f->global_descriptors, &f->instance_buffer, 1, 0);

        return true;
}

void frame_resources_destroy(FrameResources *f) {
        vkDestroySemaphore(vk_context_device(), f->swapchain_semaphore, NULL);
        vkDestroySemaphore(vk_context_device(), f->render_semaphore, NULL);

        vkDestroyFence(vk_context_device(), f->render_fence, NULL);

        vkDestroyCommandPool(vk_context_device(), f->pool, NULL);

        vkFreeDescriptorSets(vk_context_device(), g_DescriptorAllocator.pool, 1,
                             &f->global_descriptors.descriptor);
        // vkFreeDescriptorSets(vk_context_device(), g_DescriptorAllocator.pool, 1,
        // &f->mat_descriptors);

        buffer_destroy(&f->camera_uniform);
        buffer_destroy(&f->instance_buffer);
}

bool frame_resources_submit(FrameResources *f) {
        VkCommandBufferSubmitInfo command_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = f->command,
        };

        VkSemaphoreSubmitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = f->swapchain_semaphore,
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0,
            .value = 1,
        };

        VkSemaphoreSubmitInfo signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = f->render_semaphore,
            .stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            .deviceIndex = 0,
            .value = 1,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &wait_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_info,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &command_info,
        };

        VK_EXPECT(vkQueueSubmit2(vk_context_graphics_queue(), 1, &submit_info, f->render_fence));
        return true;
}

///////////////////////////////////////
/// Swapchain
///////////////////////////////////////
bool swapchain_create() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context_physical_device(),
                                                  vk_context_surface(), &capabilities);

        VkExtent2D extent;
        platform_get_size(&extent.width, &extent.height);

        VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = vk_context_surface(),
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .minImageCount = NUM_FRAMES,
            .imageFormat = g_SwapchainFormat,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };

        VK_EXPECT(vkCreateSwapchainKHR(vk_context_device(), &create_info, NULL, &g_Swapchain));

        vkGetSwapchainImagesKHR(vk_context_device(), g_Swapchain, &g_SwapchainImageCount, NULL);

        VkImage *images = malloc(sizeof(VkImage) * g_SwapchainImageCount);
        vkGetSwapchainImagesKHR(vk_context_device(), g_Swapchain, &g_SwapchainImageCount, images);

        g_SwapchainImages = malloc(sizeof(Image) * g_SwapchainImageCount);
        g_SwapchainFrameResources = malloc(sizeof(FrameResources) * g_SwapchainImageCount);

        VkExtent3D image_extent = {extent.width, extent.height, 1};

        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                ImageCreateInfo create_info = {
                    .device = vk_context_device(),
                    .image = images[i],
                    .extent = image_extent,
                    .format = g_SwapchainFormat,
                    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                };

                ASSERT(image_create(&create_info, &g_SwapchainImages[i]));
                ASSERT(frame_resources_create(&g_SwapchainFrameResources[i]));
        }

        free(images);

        return true;
}

void swapchain_destroy() {
        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                image_destroy(&g_SwapchainImages[i], vk_context_device());
                frame_resources_destroy(&g_SwapchainFrameResources[i]);
        }
        free(g_SwapchainImages);
        free(g_SwapchainFrameResources);
        vkDestroySwapchainKHR(vk_context_device(), g_Swapchain, NULL);
}

void SwapchainRecreate() {
        vkDeviceWaitIdle(vk_context_device());

        swapchain_destroy();
        swapchain_create();
}

bool swapchain_next_frame() {
        FrameResources *next = &g_SwapchainFrameResources[g_SwapchainFrameIndex];
        // todo : is it ok to expect on vkWaitForFences here? If function RV
        // represents whether or not to recreate swapchain, should you recreate if
        // fence fails (times out) or quit?
        VK_EXPECT(
            vkWaitForFences(vk_context_device(), 1, &next->render_fence, VK_TRUE, 1000000000));

        VK_EXPECT(vkAcquireNextImageKHR(vk_context_device(), g_Swapchain, 1000000000,
                                        next->swapchain_semaphore, VK_NULL_HANDLE,
                                        &g_CurrentSwapchainImageIndex));

        g_SwapchainFrameIndex = (g_SwapchainFrameIndex + 1) % g_SwapchainImageCount;
        g_CurrentFrameResources = next;
        g_CurrentSwapchainImage = &g_SwapchainImages[g_CurrentSwapchainImageIndex];

        return true;
}

///////////////////////////////////////
/// Renderer
///////////////////////////////////////
bool RendererInit(RendererCreateInfo *c) {
        platform_init(c->width, c->height, c->title);
        vk_context_init();

        ASSERT(create_vma_allocator());

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
            .allocator = g_Allocator,
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
            .allocator = g_Allocator,
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

        descriptor_allocator_destroy(&g_DescriptorAllocator);
        descriptor_layout_destroy(&g_GlobalDescriptorLayout);
        descriptor_layout_destroy(&g_IntermediateImageDescriptorLayout);
        // vkDestroyDescriptorSetLayout(vk_context_device(), g_MaterialDescriptorLayout, NULL);

        vmaDestroyAllocator(g_Allocator);

        vk_context_shutdown();
        platform_shutdown();
}

void DrawCommandClear(Image *image, VkClearColorValue color) {
        image_transition(image, g_CurrentFrameResources->command, VK_IMAGE_LAYOUT_GENERAL);
        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };
        vkCmdClearColorImage(g_CurrentFrameResources->command, image->image, image->layout, &color,
                             1, &range);
}

void DrawCommandBindCompute(ComputePipeline *compute) {
        vkCmdBindPipeline(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          compute->pipeline);
}

void DrawCommandSetComputePushConstants(ComputePipeline *compute, size_t offset, size_t size,
                                        void *data) {
        vkCmdPushConstants(g_CurrentFrameResources->command, g_GradientPipeline.layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, offset, size, data);
}

void DrawCommandBindComputeDescriptor(ComputePipeline *compute, VkDescriptorSet descriptor,
                                      uint32_t location) {
        vkCmdBindDescriptorSets(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                compute->layout, location, 1, &descriptor, 0, NULL);
}

void DrawCommandExecuteCompute(Image *image) {
        vkCmdDispatch(g_CurrentFrameResources->command, ceilf(image->extent.width / 16.0f),
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

        vkCmdBeginRendering(g_CurrentFrameResources->command, &render_info);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = image->extent.width,
            .height = image->extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(g_CurrentFrameResources->command, 0, 1, &viewport);
        vkCmdSetScissor(g_CurrentFrameResources->command, 0, 1, &scissor);
}

void DrawCommandBindGraphicsPipeline(GraphicsPipeline *graphics) {
        vkCmdBindPipeline(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics->pipeline);
        g_ActiveGraphicsPipeline = graphics;
}

void DrawCommandSetSceneData(SceneData *camera) {
        if (!g_ActiveGraphicsPipeline) {
                printf("No currently bound graphics pipeline!");
                return;
        }
        vmaCopyMemoryToAllocation(g_Allocator, camera,
                                  g_CurrentFrameResources->camera_uniform.allocation, 0,
                                  sizeof(SceneData));

        vkCmdBindDescriptorSets(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_ActiveGraphicsPipeline->layout, 0, 1,
                                &g_CurrentFrameResources->global_descriptors.descriptor, 0, NULL);
}

void DrawCommandBindTexture(Image *image) {
        if (!g_ActiveGraphicsPipeline) {
                printf("No currently bound graphics pipeline!");
                return;
        }

        vkCmdBindDescriptorSets(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_ActiveGraphicsPipeline->layout, 1, 1,
                                &g_CurrentFrameResources->mat_descriptors, 0, NULL);
}

void DrawCommandSetGraphicsPushConstants(size_t offset, size_t size, void *data) {
        vkCmdPushConstants(g_CurrentFrameResources->command, g_ActiveGraphicsPipeline->layout,
                           VK_SHADER_STAGE_VERTEX_BIT, offset, size, data);
}

void DrawCommandBindIndexBuffer(const MeshBuffer *mesh) {
        vkCmdBindIndexBuffer(g_CurrentFrameResources->command, mesh->index.buffer, 0,
                             VK_INDEX_TYPE_UINT32);
}

void DrawCommandCopyToSwapchain(Image *image) {
        image_transition(image, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        image_transition(g_CurrentSwapchainImage, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        image_blit(g_CurrentFrameResources->command, image, g_CurrentSwapchainImage);
}

bool DrawCommandBeginFrame() {
        if (!swapchain_next_frame()) {
                SwapchainRecreate();
        }

        vkResetFences(vk_context_device(), 1, &g_CurrentFrameResources->render_fence);
        begin_command_buffer(g_CurrentFrameResources->command);

        return true;
}

void DrawCommandEndFrame() {
        image_transition(g_CurrentSwapchainImage, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(g_CurrentFrameResources->command);

        frame_resources_submit(g_CurrentFrameResources);

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &g_Swapchain,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &g_CurrentFrameResources->render_semaphore,
            .pImageIndices = &g_CurrentSwapchainImageIndex,
        };
        VkResult result = vkQueuePresentKHR(vk_context_graphics_queue(), &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                printf("out of date/suboptimal swapchain.\n");
                SwapchainRecreate();
        }

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
            .allocator = g_Allocator,

            .data = (uint32_t *)mats->diffuse_tex,
            .imm = &g_ImmediateCommand,
        };

        uint32_t index = array_length(g_Textures);
        array_append(g_Textures, (AllocatedImage){0});
        AllocatedImage *image = &g_Textures[index];
        allocated_image_create(&create_info, image);

        for (int i = 0; i < g_SwapchainImageCount; i += 1) {
                descriptor_write_texture(g_SwapchainFrameResources[i].global_descriptors,
                                         &g_Textures[index].image, 2, index, g_LinearSampler);
        }

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
        descriptor_allocator_init(&g_DescriptorAllocator, NUM_FRAMES);

        DescriptorBinding draw_image_bindings[1] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .count = 1,
             .stage = VK_SHADER_STAGE_COMPUTE_BIT},
        };

        descriptor_allocator_reserve(&g_DescriptorAllocator, draw_image_bindings, 1, false);
        g_IntermediateImageDescriptorLayout =
            descriptor_layout_create(vk_context_device(), draw_image_bindings, 1);

        DescriptorBinding global_bindings[3] = {
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .count = 1,
             .stage = VK_SHADER_STAGE_ALL_GRAPHICS},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             .count = 1,
             .stage = VK_SHADER_STAGE_VERTEX_BIT},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .count = MAX_TEXTURES,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .variable = true},
        };

        descriptor_allocator_reserve(&g_DescriptorAllocator, global_bindings, 3, true);
        g_GlobalDescriptorLayout =
            descriptor_layout_create(vk_context_device(), global_bindings, 3);

        ASSERT(descriptor_allocator_create(vk_context_device(), &g_DescriptorAllocator));
        g_IntermediateImageDescriptors =
            descriptor_allocate(&g_DescriptorAllocator, &g_IntermediateImageDescriptorLayout);

        descriptor_write_image(g_IntermediateImageDescriptors, &g_IntermediateImage.image, 0, 0);

        return true;
}

bool create_vma_allocator() {
        VmaVulkanFunctions functions = {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        };

        VmaAllocatorCreateInfo create_info = {
            .physicalDevice = vk_context_physical_device(),
            .instance = vk_context_instance(),
            .device = vk_context_device(),
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .pVulkanFunctions = &functions,
        };

        VK_EXPECT(vmaCreateAllocator(&create_info, &g_Allocator));
        return true;
}

bool begin_command_buffer(VkCommandBuffer command) {
        VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_EXPECT(vkResetCommandBuffer(command, 0));
        VK_EXPECT(vkBeginCommandBuffer(command, &info));

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
        vkCmdDrawIndexed(g_CurrentFrameResources->command, mesh->num_indices, batch->count, 0, 0,
                         batch->first_instance);
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
        image_transition(&g_IntermediateImage.image, g_CurrentFrameResources->command,
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
        image_transition(&g_IntermediateImage.image, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        image_transition(&g_DepthImage.image, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        DrawCommandBeginRendering(&g_IntermediateImage.image);
}

void agpu_end_frame() {
        Instance *ssbo =
            (Instance *)buffer_mmap(&g_CurrentFrameResources->instance_buffer, g_Allocator);

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

        buffer_munmap(&g_CurrentFrameResources->instance_buffer, g_Allocator);

        vkCmdEndRendering(g_CurrentFrameResources->command);

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
