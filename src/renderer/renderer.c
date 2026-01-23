#include "renderer.h"

#include "buffer.h"
#include "command.h"
#include "descriptors.h"
#include "image.h"
#include "material.h"
#include "pipeline.h"

#include "common/array.h"
#include "common/util.h"

#include <SDL3/SDL_vulkan.h>

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

SDL_Window *g_Window;
VkSurfaceKHR g_Surface;

VkInstance g_Instance;
VkDebugUtilsMessengerEXT g_DebugMessenger;

VkPhysicalDevice g_PhysicalDevice;

VkDevice g_Device;
uint32_t g_QueueFamilyIndex;
VkQueue g_GraphicsQueue;

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

RenderObject g_RedDemons[10];
RenderObject g_StoneGolems[10];

vec3 g_CameraPosition = {0.0f, 0.0f, 2.0f};
vec3 g_CameraViewDirection = {0.0f, 0.0f, -1.0f};
const vec3 g_UpVector = {0.0f, 1.0f, 0.0f};

uint32_t g_Width;
uint32_t g_Height;

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
        buffer->vertex_address = vkGetBufferDeviceAddress(g_Device, &address_info);

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
            .queueFamilyIndex = g_QueueFamilyIndex,
        };
        VK_EXPECT(vkCreateCommandPool(g_Device, &command_pool_info, NULL, &f->pool));

        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = f->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK_EXPECT(vkAllocateCommandBuffers(g_Device, &alloc_info, &f->command));

        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_EXPECT(vkCreateFence(g_Device, &fence_info, NULL, &f->render_fence));

        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = 0,
        };
        VK_EXPECT(vkCreateSemaphore(g_Device, &semaphore_info, NULL, &f->swapchain_semaphore));
        VK_EXPECT(vkCreateSemaphore(g_Device, &semaphore_info, NULL, &f->render_semaphore));

        EXPECT(buffer_create(g_Allocator, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU, &f->camera_uniform));
        EXPECT(buffer_create(g_Allocator, sizeof(Instance) * MAX_INSTANCES,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, &f->instance_buffer));

        f->global_descriptors =
            descriptor_allocate(&g_DescriptorAllocator, &g_GlobalDescriptorLayout);

        descriptor_write_buffer(f->global_descriptors, &f->camera_uniform, 0, 0);
        descriptor_write_buffer(f->global_descriptors, &f->instance_buffer, 1, 0);

        return true;
}

void frame_resources_destroy(FrameResources *f) {
        vkDestroySemaphore(g_Device, f->swapchain_semaphore, NULL);
        vkDestroySemaphore(g_Device, f->render_semaphore, NULL);

        vkDestroyFence(g_Device, f->render_fence, NULL);

        vkDestroyCommandPool(g_Device, f->pool, NULL);

        vkFreeDescriptorSets(g_Device, g_DescriptorAllocator.pool, 1,
                             &f->global_descriptors.descriptor);
        // vkFreeDescriptorSets(g_Device, g_DescriptorAllocator.pool, 1, &f->mat_descriptors);

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

        VK_EXPECT(vkQueueSubmit2(g_GraphicsQueue, 1, &submit_info, f->render_fence));
        return true;
}

///////////////////////////////////////
/// Swapchain
///////////////////////////////////////
bool swapchain_create() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);

        VkExtent2D extent = {g_Width, g_Height};

        VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = g_Surface,
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

        VK_EXPECT(vkCreateSwapchainKHR(g_Device, &create_info, NULL, &g_Swapchain));

        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &g_SwapchainImageCount, NULL);

        VkImage *images = malloc(sizeof(VkImage) * g_SwapchainImageCount);
        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &g_SwapchainImageCount, images);

        g_SwapchainImages = malloc(sizeof(Image) * g_SwapchainImageCount);
        g_SwapchainFrameResources = malloc(sizeof(FrameResources) * g_SwapchainImageCount);

        VkExtent3D image_extent = {extent.width, extent.height, 1};

        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                ImageCreateInfo create_info = {
                    .device = g_Device,
                    .image = images[i],
                    .extent = image_extent,
                    .format = g_SwapchainFormat,
                    .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
                };

                EXPECT(image_create(&create_info, &g_SwapchainImages[i]));
                EXPECT(frame_resources_create(&g_SwapchainFrameResources[i]));
        }

        return true;
}

void swapchain_destroy() {
        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                image_destroy(&g_SwapchainImages[i], g_Device);
                frame_resources_destroy(&g_SwapchainFrameResources[i]);
        }
        free(g_SwapchainImages);
        free(g_SwapchainFrameResources);
        vkDestroySwapchainKHR(g_Device, g_Swapchain, NULL);
}

void SwapchainRecreate() {
        vkDeviceWaitIdle(g_Device);

        swapchain_destroy();
        swapchain_create();
}

bool swapchain_next_frame() {
        FrameResources *next = &g_SwapchainFrameResources[g_SwapchainFrameIndex];
        // todo : is it ok to expect on vkWaitForFences here? If function RV
        // represents whether or not to recreate swapchain, should you recreate if
        // fence fails (times out) or quit?
        VK_EXPECT(vkWaitForFences(g_Device, 1, &next->render_fence, VK_TRUE, 1000000000));

        VK_EXPECT(vkAcquireNextImageKHR(g_Device, g_Swapchain, 1000000000,
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
        VK_EXPECT(volkInitialize());

        if (!SDL_Init(SDL_INIT_VIDEO)) {
                printf("Failed to initialize SDL: %s\n", SDL_GetError());
                return false;
        }

        g_Window = SDL_CreateWindow(c->title, c->width, c->height,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
        if (!g_Window) {
                printf("Failed to create Window: %s\n", SDL_GetError());
                return false;
        }

        int w, h;
        SDL_GetWindowSize(g_Window, &w, &h);

        printf("w: %d, h: %d\n", w, h);

        g_Width = w;
        g_Height = h;

        EXPECT(create_instance(c));
        EXPECT(create_debug_messenger());
        EXPECT(create_surface());

        EXPECT(select_gpu());
        EXPECT(create_device());
        EXPECT(create_vma_allocator());

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
            .device = g_Device,
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
            .device = g_Device,
        };

        EXPECT(allocated_image_create(&allocated_image_info, &g_IntermediateImage));
        EXPECT(allocated_image_create(&depth_image_info, &g_DepthImage));

        create_samplers();

        EXPECT(init_descriptors());

        EXPECT(swapchain_create());

        EXPECT(immediate_command_create(g_Device, g_QueueFamilyIndex, g_GraphicsQueue,
                                        &g_ImmediateCommand));

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
        EXPECT(compute_pipeline_create(g_Device, &pipeline_info, &g_GradientPipeline));

        free(gradient_comp);

        g_PbrPipeline =
            create_pbr_pipeline(g_Device, g_GlobalDescriptorLayout.layout,
                                g_MaterialDescriptorLayout, g_IntermediateImage.image.format);

        g_MeshBuffers = array(MeshBuffer);
        g_Textures = array(AllocatedImage);

        // g_Model = load_model("assets/objs/city/center-city/Center_City_Sci-Fi.obj");
        // g_Model = load_model("assets/objs/red-demon-tris.obj");
        Model red_demon = load_model("assets/bard-1.obj");
        g_RedDemons[0] = gpu_upload_model(&red_demon);

        Model stone_golem = load_model("assets/bard-1.obj");
        g_StoneGolems[0] = gpu_upload_model(&stone_golem);

        for (int i = 0; i < 10; i += 1) {
                g_RedDemons[i] = g_RedDemons[0];
                g_StoneGolems[i] = g_StoneGolems[0];

                g_RedDemons[i].transform = mat4_translate(MAT4_IDENTITY, (vec3){1, 0, -i});
                g_StoneGolems[i].transform = mat4_translate(MAT4_IDENTITY, (vec3){-1, 0, -i});
        }

        model_destroy(red_demon);
        model_destroy(stone_golem);

        return true;
}

void RendererShutdown() {
        // Make sure the GPU has finished all work
        vkDeviceWaitIdle(g_Device);

        compute_pipeline_destroy(&g_GradientPipeline, g_Device);
        graphics_pipeline_destroy(&g_PbrPipeline, g_Device);

        vkDestroySampler(g_Device, g_LinearSampler, NULL);
        vkDestroySampler(g_Device, g_NearestSampler, NULL);

        allocated_image_destroy(&g_IntermediateImage, g_Device);
        allocated_image_destroy(&g_DepthImage, g_Device);

        for (int i = 0; i < array_length(g_MeshBuffers); i += 1) {
                mesh_buffer_destroy(&g_MeshBuffers[i]);
        }
        array_free(g_MeshBuffers);

        for (int i = 0; i < array_length(g_Textures); i += 1) {
                allocated_image_destroy(&g_Textures[i], g_Device);
        }
        array_free(g_Textures);

        swapchain_destroy();
        immediate_command_destroy(&g_ImmediateCommand);

        descriptor_allocator_destroy(&g_DescriptorAllocator);
        descriptor_layout_destroy(&g_GlobalDescriptorLayout);
        descriptor_layout_destroy(&g_IntermediateImageDescriptorLayout);
        // vkDestroyDescriptorSetLayout(g_Device, g_MaterialDescriptorLayout, NULL);

        vmaDestroyAllocator(g_Allocator);

        vkDestroyDevice(g_Device, NULL);

        vkDestroySurfaceKHR(g_Instance, g_Surface, NULL);
        vkDestroyDebugUtilsMessengerEXT(g_Instance, g_DebugMessenger, NULL);
        vkDestroyInstance(g_Instance, NULL);

        SDL_DestroyWindow(g_Window);
        SDL_Quit();
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
                vkDeviceWaitIdle(g_Device);

                swapchain_destroy();
                swapchain_create();

                return false;
        }

        vkResetFences(g_Device, 1, &g_CurrentFrameResources->render_fence);
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
        VkResult result = vkQueuePresentKHR(g_GraphicsQueue, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                printf("out of date/suboptimal swapchain.\n");
                SwapchainRecreate();
        }

        // Refresh the window
        SDL_UpdateWindowSurface(g_Window);
}

void RendererDraw() {
        int w, h;
        SDL_GetWindowSize(g_Window, &w, &h);

        if (w != g_Width || h != g_Height) {
                printf("Resizing Swapchain: w = %d, h = %d\n", w, h);
                g_Width = w;
                g_Height = h;
                SwapchainRecreate();
        }

        if (!DrawCommandBeginFrame()) {
                return;
        }
        DrawCommandClear(&g_IntermediateImage.image, (VkClearColorValue){0.0f, 0.0f, 1.0f, 0.0f});

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

        // mesh pipeline
        DrawCommandBindGraphicsPipeline(&g_PbrPipeline);

        mat4 view = mat4_look_at(g_CameraPosition,
                                 vec3_add(g_CameraPosition, g_CameraViewDirection), g_UpVector);
        mat4 proj = mat4_perspective(to_radians(45.0f), (float)g_Width / g_Height, 0.1f, 100.0f);
        mat4 viewproj = mat4_mul(proj, view);

        SceneData scene = {
            .view = view,
            .proj = view,
            .viewproj = viewproj,
            .ambientColor = (vec4){1.0f, 1.0f, 1.0f, 0.0f},
            .sunlightDirection = (vec4){0.3f, 1.0f, 0.3f, 0.1f},
        };
        DrawCommandSetSceneData(&scene);

        Instance *ssbo =
            (Instance *)buffer_mmap(&g_CurrentFrameResources->instance_buffer, g_Allocator);

        DrawBatch golem_batch = draw_batch_create(g_StoneGolems, 0);

        for (int i = 0; i < 10; i += 1) {
                draw_batch_add(&golem_batch, &g_StoneGolems[i], ssbo);
        }

        DrawBatch demon_batch = draw_batch_create(g_RedDemons, golem_batch.count);

        for (int i = 0; i < 10; i += 1) {
                draw_batch_add(&demon_batch, &g_RedDemons[i], ssbo);
        }

        buffer_munmap(&g_CurrentFrameResources->instance_buffer, g_Allocator);

        draw_batch_draw(&golem_batch);
        draw_batch_draw(&demon_batch);

        vkCmdEndRendering(g_CurrentFrameResources->command);

        DrawCommandCopyToSwapchain(&g_IntermediateImage.image);
        DrawCommandEndFrame();
}

void MoveCamera(vec3 delta) {
        // TODO: relative movement should be delta rotated by g_CameraViewDirection
        vec3 relative = {delta.x, delta.y, -delta.z};
        g_CameraPosition = vec3_add(g_CameraPosition, relative);
}

uint32_t gpu_upload_texture(MaterialInfo *mats) {
        AllocatedImageCreateInfo create_info = {
            .extent = (VkExtent3D){mats->diffuse_width, mats->diffuse_height, 1},
            .aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .memory_usage = VMA_MEMORY_USAGE_GPU_ONLY,

            .device = g_Device,
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

RenderObject gpu_upload_model(Model *model) {
        RenderObject r;

        r.mesh = mesh_buffer_create(model->meshes);
        r.material = &g_DefaultMaterial;
        r.tex_index = gpu_upload_texture(model->materials);

        return r;
}

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        printf("[Validation]: %s\n", pCallbackData->pMessage);

        return VK_FALSE;
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
            descriptor_layout_create(g_Device, draw_image_bindings, 1);

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
        g_GlobalDescriptorLayout = descriptor_layout_create(g_Device, global_bindings, 3);

        EXPECT(descriptor_allocator_create(g_Device, &g_DescriptorAllocator));
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
            .physicalDevice = g_PhysicalDevice,
            .instance = g_Instance,
            .device = g_Device,
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .pVulkanFunctions = &functions,
        };

        VK_EXPECT(vmaCreateAllocator(&create_info, &g_Allocator));
        return true;
}

bool create_instance(RendererCreateInfo *c) {
        VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                      .pApplicationName = c->title,
                                      .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
                                      .pEngineName = "No Engine",
                                      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                      .apiVersion = VK_API_VERSION_1_3};

        VkDebugUtilsMessengerCreateInfoEXT validation_callback = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = validation_message_callback,
        };

        uint32_t count = 0;
        const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);

        printf("SDL Extensions:\n");
        for (uint32_t i = 0; i < count; i += 1) {
                printf("  - %s\n", sdl_extensions[i]);
        }

        const char **extensions = malloc(sizeof(const char *) * (count + 1));
        for (uint32_t i = 0; i < count; i += 1) {
                extensions[i] = sdl_extensions[i];
        }
        extensions[count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

        const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

        VkInstanceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                            .pApplicationInfo = &app_info,
                                            .enabledExtensionCount = count + 1,
                                            .ppEnabledExtensionNames = extensions,
                                            .enabledLayerCount = 1,
                                            .ppEnabledLayerNames = layers,
                                            .pNext = &validation_callback};

        VK_EXPECT(vkCreateInstance(&create_info, NULL, &g_Instance));
        volkLoadInstance(g_Instance);

        free(extensions);
        return true;
}

bool create_debug_messenger() {
        VkDebugUtilsMessengerCreateInfoEXT create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = validation_message_callback,
        };

        VK_EXPECT(
            vkCreateDebugUtilsMessengerEXT(g_Instance, &create_info, NULL, &g_DebugMessenger));
        return true;
}

bool create_surface() {
        if (!SDL_Vulkan_CreateSurface(g_Window, g_Instance, NULL, &g_Surface)) {
                printf("SDL failed to create window: %s.\n", SDL_GetError());
                return false;
        }

        return true;
}

bool is_gpu_suitable(VkPhysicalDevice gpu) {
        VkPhysicalDeviceProperties p;
        VkPhysicalDeviceVulkan12Features f12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &f12};
        VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                       .pNext = &f13};

        vkGetPhysicalDeviceProperties(gpu, &p);
        vkGetPhysicalDeviceFeatures2(gpu, &f);

        if (p.apiVersion < VK_API_VERSION_1_3) {
                return false;
        }

        if (!f12.bufferDeviceAddress || !f12.descriptorIndexing || !f13.dynamicRendering ||
            !f13.synchronization2 || !f.features.robustBufferAccess ||
            !f12.descriptorBindingPartiallyBound || !f12.runtimeDescriptorArray) {
                return false;
        }

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);

        VkQueueFamilyProperties *properties = malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, properties);

        for (uint32_t i = 0; i < count; i += 1) {
                VkBool32 support;
                VK_EXPECT(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, g_Surface, &support));

                if (support && properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        g_QueueFamilyIndex = i;
                        printf("Selected Device: %s.\n", p.deviceName);
                }
        }

        free(properties);
        return true;
}

bool select_gpu() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(g_Instance, &count, NULL);

        if (count == 0) {
                printf("Failed to find GPUs with Vulkan support.\n");
                return false;
        }

        VkPhysicalDevice *gpus = malloc(sizeof(VkPhysicalDevice *) * count);
        vkEnumeratePhysicalDevices(g_Instance, &count, gpus);

        for (uint32_t i = 0; i < count; i += 1) {
                if (is_gpu_suitable(gpus[i])) {
                        g_PhysicalDevice = gpus[i];
                        break;
                }
        }

        free(gpus);
        return true;
}

bool create_device() {
        float priorities[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = g_QueueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities,
        };

        const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkPhysicalDeviceVulkan12Features f12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .bufferDeviceAddress = VK_TRUE,
            .descriptorIndexing = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        };
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .dynamicRendering = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .pNext = &f12,
        };
        VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                       .features.robustBufferAccess = VK_TRUE,
                                       .pNext = &f13};

        VkDeviceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pQueueCreateInfos = &queue_info,
            .queueCreateInfoCount = 1,
            .ppEnabledExtensionNames = extensions,
            .enabledExtensionCount = 1,
            .pNext = &f,
        };

        VK_EXPECT(vkCreateDevice(g_PhysicalDevice, &create_info, NULL, &g_Device));
        volkLoadDevice(g_Device);

        vkGetDeviceQueue(g_Device, g_QueueFamilyIndex, 0, &g_GraphicsQueue);

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

        vkCreateSampler(g_Device, &sampler_info, NULL, &g_LinearSampler);

        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;

        vkCreateSampler(g_Device, &sampler_info, NULL, &g_NearestSampler);
}

DrawBatch draw_batch_create(RenderObject *obj, uint32_t first_instance) {
        DrawBatch batch = {0};
        batch.material = obj->material;
        batch.mesh = obj->mesh;
        batch.first_instance = first_instance;

        return batch;
}

void draw_batch_add(DrawBatch *batch, RenderObject *obj, Instance *ssbo) {
        uint32_t index = batch->first_instance + batch->count;
        batch->count += 1;
        const MeshBuffer *mesh = &g_MeshBuffers[batch->mesh];

        ssbo[index] = (Instance){
            .model = obj->transform,
            .vertex_address = mesh->vertex_address,
            .tex_index = obj->tex_index,
        };
}

void draw_batch_draw(DrawBatch *batch) {
        const MeshBuffer *mesh = &g_MeshBuffers[batch->mesh];
        DrawCommandBindIndexBuffer(mesh);
        vkCmdDrawIndexed(g_CurrentFrameResources->command, mesh->num_indices, batch->count, 0, 0,
                         batch->first_instance);
}
