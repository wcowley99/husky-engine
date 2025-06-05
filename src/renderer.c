#include "renderer.h"

#include "colored_triangle_frag.h"
#include "colored_triangle_mesh_vert.h"
#include "colored_triangle_vert.h"
#include "gradient2_comp.h"

#include <SDL3/SDL_vulkan.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

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

// A reference to the current swapchain image
Image *g_CurrentSwapchainImage;

// A reference to the resources of the current swapchain frame
FrameResources *g_CurrentFrameResources;

// The current swapchain image index
uint32_t g_CurrentSwapchainImageIndex;

VmaAllocator g_Allocator;
ImmediateCommand g_ImmediateCommand;

DescriptorAllocator g_DescriptorAllocator;

VkDescriptorSetLayout g_GlobalDescriptorLayout;

AllocatedImage g_IntermediateImage;
VkDescriptorSetLayout g_IntermediateImageDescriptorLayout;
VkDescriptorSet g_IntermediateImageDescriptors;

ComputePipeline g_GradientPipeline;
GraphicsPipeline g_TrianglePipeline;
GraphicsPipeline g_MeshPipeline;

GraphicsPipeline *g_ActiveGraphicsPipeline;

MeshBuffer g_RectangleMesh;

vec3 g_CameraPosition = {0.0f, 0.0f, 2.0f};
vec3 g_CameraViewDirection = {0.0f, 0.0f, -1.0f};
const vec3 g_UpVector = {0.0f, 1.0f, 0.0f};

uint32_t g_Width;
uint32_t g_Height;

bool buffer_create(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage usage, Buffer *buffer) {
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = flags,
        };

        VmaAllocationCreateInfo alloc_info = {
            .usage = usage,
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        };

        VkResult result = vmaCreateBuffer(g_Allocator, &create_info, &alloc_info, &buffer->buffer,
                                          &buffer->allocation, &buffer->info);

        if (result != VK_SUCCESS) {
                printf("Failed to create buffer.\n");
                return false;
        } else {
                return true;
        }
}

void buffer_destroy(Buffer *buffer) {
        vmaDestroyBuffer(g_Allocator, buffer->buffer, buffer->allocation);
}

bool mesh_buffer_create(Mesh *mesh, MeshBuffer *buffer) {
        const size_t vertex_buffer_size = sizeof(Vertex) * mesh->num_vertices;
        const size_t index_buffer_size = sizeof(uint32_t) * mesh->num_indices;

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
        buffer->vertex_address = vkGetBufferDeviceAddress(g_Device, &address_info);

        Buffer staging_buffer;
        buffer_create(vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_ONLY, &staging_buffer);
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
        return true;
}

void mesh_buffer_destroy(MeshBuffer *buffer) {
        buffer_destroy(&buffer->vertex);
        buffer_destroy(&buffer->index);
}

///////////////////////////////////////
/// Image
///////////////////////////////////////

bool image_create(VkImage vk_image, VkExtent2D extent, VkFormat format, VkImageLayout layout,
                  Image *image) {
        image->image = vk_image;

        image->extent.width = extent.width;
        image->extent.height = extent.height;
        image->extent.depth = 1;

        image->format = format;
        image->layout = layout;

        VkComponentMapping mapping = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        };

        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .baseMipLevel = 0,
            .levelCount = 1,
        };

        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk_image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = mapping,
            .subresourceRange = range,
        };

        VK_EXPECT(vkCreateImageView(g_Device, &create_info, NULL, &image->image_view));
        return true;
}

void image_destroy(Image *image) { vkDestroyImageView(g_Device, image->image_view, NULL); }

void image_transition(Image *image, VkCommandBuffer command, VkImageLayout layout) {
        VkImageAspectFlags mask = (layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                                      ? VK_IMAGE_ASPECT_DEPTH_BIT
                                      : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageSubresourceRange range = {
            .aspectMask = mask,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };

        VkImageMemoryBarrier2 barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .oldLayout = image->layout,
            .newLayout = layout,
            .subresourceRange = range,
            .image = image->image,
        };

        VkDependencyInfo depInfo = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                    .imageMemoryBarrierCount = 1,
                                    .pImageMemoryBarriers = &barrier};

        vkCmdPipelineBarrier2(command, &depInfo);

        image->layout = layout;
}

void image_blit(VkCommandBuffer command, Image *src, Image *dst) {
        VkImageBlit2 blit = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcOffsets =
                {{0}, {.x = src->extent.width, .y = src->extent.height, .z = src->extent.depth}},
            .dstOffsets =
                {{0}, {.x = dst->extent.width, .y = dst->extent.height, .z = dst->extent.depth}},
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                    .mipLevel = 0,
                },
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                    .mipLevel = 0,
                },
        };

        VkBlitImageInfo2 blit_info = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = src->image,
            .srcImageLayout = src->layout,
            .dstImage = dst->image,
            .dstImageLayout = dst->layout,
            .filter = VK_FILTER_LINEAR,
            .regionCount = 1,
            .pRegions = &blit,
        };

        vkCmdBlitImage2(command, &blit_info);
}

bool allocated_image_create(AllocatedImageCreateInfo *info, AllocatedImage *image) {
        VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = info->format,
            .extent = info->extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = info->usage_flags,
        };

        VmaAllocationCreateInfo alloc_info = {
            .usage = info->memory_usage,
            .requiredFlags = info->memory_props,
        };

        VkImage raw_image;
        VK_EXPECT(vmaCreateImage(g_Allocator, &create_info, &alloc_info, &raw_image,
                                 &image->allocation, NULL));

        VkExtent2D extent = {
            .width = info->extent.width,
            .height = info->extent.height,
        };
        EXPECT(image_create(raw_image, extent, info->format, VK_IMAGE_LAYOUT_UNDEFINED,
                            &image->image));

        return true;
}

void allocated_image_destroy(AllocatedImage *image) {
        image_destroy(&image->image);
        vmaDestroyImage(g_Allocator, image->image.image, image->allocation);
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

        EXPECT(descriptor_allocator_allocate(&g_DescriptorAllocator, &g_GlobalDescriptorLayout, 1,
                                             &f->global_descriptors));

        EXPECT(buffer_create(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU, &f->camera_uniform));

        VkDescriptorBufferInfo camera_info = {
            .buffer = f->camera_uniform.buffer,
            .offset = 0,
            .range = sizeof(CameraData),
        };

        VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .dstSet = f->global_descriptors,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camera_info,
        };

        vkUpdateDescriptorSets(g_Device, 1, &write_set, 0, NULL);

        return true;
}

void frame_resources_destroy(FrameResources *f) {
        vkDestroySemaphore(g_Device, f->swapchain_semaphore, NULL);
        vkDestroySemaphore(g_Device, f->render_semaphore, NULL);

        vkDestroyFence(g_Device, f->render_fence, NULL);

        vkDestroyCommandPool(g_Device, f->pool, NULL);

        buffer_destroy(&f->camera_uniform);
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
/// Compute Pipeline
///////////////////////////////////////

bool compute_pipeline_create(ComputePipelineInfo *info, ComputePipeline *p) {
        VkPushConstantRange *ranges =
            malloc(info->num_push_constant_sizes * sizeof(VkPushConstantRange));

        for (uint32_t i = 0; i < info->num_push_constant_sizes; i += 1) {
                ranges[i].size = info->push_constant_sizes[i];
                ranges[i].offset = 0;
                ranges[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkPipelineLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = info->descriptors,
            .setLayoutCount = info->num_descriptors,
            .pPushConstantRanges = ranges,
            .pushConstantRangeCount = info->num_push_constant_sizes,
        };

        VkResult result = vkCreatePipelineLayout(g_Device, &layout_create_info, NULL, &p->layout);
        free(ranges);
        VK_EXPECT(result);

        VkShaderModule compute;
        EXPECT(create_shader_module(info->shader_source, info->shader_source_size, &compute));

        VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = compute,
                    .pName = "main",
                },
            .layout = p->layout,
        };

        result = vkCreateComputePipelines(g_Device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                          &p->pipeline);
        vkDestroyShaderModule(g_Device, compute, NULL);
        VK_EXPECT(result);
        return true;
}

void compute_pipeline_destroy(ComputePipeline *p) {
        vkDestroyPipelineLayout(g_Device, p->layout, NULL);
        vkDestroyPipeline(g_Device, p->pipeline, NULL);
}

///////////////////////////////////////
/// Graphics Pipeline
///////////////////////////////////////

bool graphics_pipeline_create(GraphicsPipelineCreateInfo *create_info, GraphicsPipeline *pipeline) {
        VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineColorBlendAttachmentState color_attachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blend = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
        };

        VkPipelineDepthStencilStateCreateInfo depth_testing = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthCompareOp = VK_COMPARE_OP_NEVER,
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };

        VkPipelineVertexInputStateCreateInfo vertex_input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .primitiveRestartEnable = VK_FALSE,
            .topology = create_info->topology,
        };

        VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        VkPipelineRenderingCreateInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &create_info->color_attachment_format,
            .depthAttachmentFormat = create_info->depth_attachment_format,
        };

        VkPipelineRasterizationStateCreateInfo rasterization = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = create_info->polygon_mode,
            .lineWidth = 1.0f,
            .cullMode = create_info->cull_mode,
            .frontFace = create_info->front_face,
        };

        VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = state,
        };

        VkShaderModule vertex;
        VkShaderModule fragment;
        EXPECT(create_shader_module(create_info->vertex_shader, create_info->vertex_shader_size,
                                    &vertex));
        EXPECT(create_shader_module(create_info->fragment_shader, create_info->fragment_shader_size,
                                    &fragment));

        VkPipelineShaderStageCreateInfo shaders[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertex,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment,
                .pName = "main",
            },
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = create_info->descriptors,
            .setLayoutCount = create_info->num_descriptors,
            .pPushConstantRanges = create_info->push_constants,
            .pushConstantRangeCount = create_info->num_push_constants,
        };

        VK_EXPECT(vkCreatePipelineLayout(g_Device, &layout_info, NULL, &pipeline->layout));

        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaders,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisample,
            .pColorBlendState = &color_blend,
            .pDepthStencilState = &depth_testing,
            .layout = pipeline->layout,
            .pDynamicState = &dynamic_state,
            .pNext = &render_info,
        };

        VK_EXPECT(vkCreateGraphicsPipelines(g_Device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                            &pipeline->pipeline));

        vkDestroyShaderModule(g_Device, vertex, NULL);
        vkDestroyShaderModule(g_Device, fragment, NULL);

        return true;
}

void graphics_pipeline_destroy(GraphicsPipeline *pipeline) {
        vkDestroyPipelineLayout(g_Device, pipeline->layout, NULL);
        vkDestroyPipeline(g_Device, pipeline->pipeline, NULL);
}

///////////////////////////////////////
/// Swapchain
///////////////////////////////////////
bool swapchain_create() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);

        VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = g_Surface,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .minImageCount = capabilities.minImageCount + 1,
            .imageFormat = g_SwapchainFormat,
            .imageExtent = capabilities.currentExtent,
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

        g_Width = capabilities.currentExtent.width;
        g_Height = capabilities.currentExtent.height;

        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &g_SwapchainImageCount, NULL);

        VkImage *images = malloc(sizeof(VkImage) * g_SwapchainImageCount);
        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &g_SwapchainImageCount, images);

        g_SwapchainImages = malloc(sizeof(Image) * g_SwapchainImageCount);
        g_SwapchainFrameResources = malloc(sizeof(FrameResources) * g_SwapchainImageCount);
        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                EXPECT(image_create(images[i], capabilities.currentExtent, g_SwapchainFormat,
                                    VK_IMAGE_LAYOUT_UNDEFINED, &g_SwapchainImages[i]));
                EXPECT(frame_resources_create(&g_SwapchainFrameResources[i]));
        }

        return true;
}

void swapchain_destroy() {
        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                image_destroy(&g_SwapchainImages[i]);
                frame_resources_destroy(&g_SwapchainFrameResources[i]);
        }
        free(g_SwapchainImages);
        free(g_SwapchainFrameResources);
        vkDestroySwapchainKHR(g_Device, g_Swapchain, NULL);
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
/// Descriptor Allocator
///////////////////////////////////////

bool descriptor_allocator_create(DescriptorAllocator *allocator) {
        VkDescriptorPoolSize pool_sizes[] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 4},
        };
        EXPECT(create_descriptor_pool(pool_sizes, sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
                                      &allocator->pool));

        return true;
}

void descriptor_allocator_destroy(DescriptorAllocator *allocator) {
        vkDestroyDescriptorPool(g_Device, allocator->pool, NULL);
}

void descriptor_allocator_clear(DescriptorAllocator *allocator) {
        vkResetDescriptorPool(g_Device, allocator->pool, 0);
}

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDescriptorSetLayout *layouts,
                                   uint32_t count, VkDescriptorSet *sets) {
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = allocator->pool,
            .pSetLayouts = layouts,
            .descriptorSetCount = count,
        };

        VK_EXPECT(vkAllocateDescriptorSets(g_Device, &alloc_info, sets));
        return true;
}

///////////////////////////////////////
/// Immediate Command
///////////////////////////////////////

bool immediate_command_create(ImmediateCommand *command) {
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0,
        };
        VK_EXPECT(vkCreateFence(g_Device, &fence_info, NULL, &command->fence));

        VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = g_QueueFamilyIndex,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        VK_EXPECT(vkCreateCommandPool(g_Device, &command_pool_info, NULL, &command->pool));

        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = command->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };
        VK_EXPECT(vkAllocateCommandBuffers(g_Device, &alloc_info, &command->command));

        return true;
}

void immediate_command_destroy(ImmediateCommand *command) {
        vkDestroyCommandPool(g_Device, command->pool, NULL);
        vkDestroyFence(g_Device, command->fence, NULL);
}

void immediate_command_begin(ImmediateCommand *command) {
        VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(command->command, &begin);
}

void immediate_command_end(ImmediateCommand *command) {
        vkEndCommandBuffer(command->command);

        VkCommandBufferSubmitInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = command->command,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pCommandBufferInfos = &cmd_info,
            .commandBufferInfoCount = 1,
        };
        vkQueueSubmit2(g_GraphicsQueue, 1, &submit_info, command->fence);
        vkWaitForFences(g_Device, 1, &command->fence, VK_TRUE, 1000000000);

        vkResetFences(g_Device, 1, &command->fence);
        vkResetCommandPool(g_Device, command->pool, 0);
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
            .usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        };

        EXPECT(allocated_image_create(&allocated_image_info, &g_IntermediateImage));

        EXPECT(init_descriptors());

        EXPECT(swapchain_create());

        EXPECT(immediate_command_create(&g_ImmediateCommand));

        uint32_t sizes[] = {sizeof(float) * 16};
        ComputePipelineInfo pipeline_info = {
            .descriptors = &g_IntermediateImageDescriptorLayout,
            .num_descriptors = 1,
            .push_constant_sizes = sizes,
            .num_push_constant_sizes = 1,
            .shader_source = (const uint32_t *)gradient2_comp_file,
            .shader_source_size = gradient2_comp_size / 4,
        };
        EXPECT(compute_pipeline_create(&pipeline_info, &g_GradientPipeline));

        GraphicsPipelineCreateInfo graphics_pipeline_info = {
            .vertex_shader = (const uint32_t *)colored_triangle_vert_file,
            .vertex_shader_size = colored_triangle_vert_size / 4,
            .fragment_shader = (const uint32_t *)colored_triangle_frag_file,
            .fragment_shader_size = colored_triangle_frag_size / 4,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .polygon_mode = VK_POLYGON_MODE_FILL,
            .cull_mode = VK_CULL_MODE_NONE,
            .front_face = VK_FRONT_FACE_CLOCKWISE,
            .color_attachment_format = g_IntermediateImage.image.format,
            .depth_attachment_format = VK_FORMAT_UNDEFINED,
        };
        EXPECT(graphics_pipeline_create(&graphics_pipeline_info, &g_TrianglePipeline));

        VkPushConstantRange push_constants[] = {
            {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(MeshPushConstants),
            },
        };
        GraphicsPipelineCreateInfo mesh_pipeline_info = {
            .descriptors = &g_GlobalDescriptorLayout,
            .num_descriptors = 1,
            .push_constants = push_constants,
            .num_push_constants = 1,
            .vertex_shader = (const uint32_t *)colored_triangle_mesh_vert_file,
            .vertex_shader_size = colored_triangle_mesh_vert_size / 4,
            .fragment_shader = (const uint32_t *)colored_triangle_frag_file,
            .fragment_shader_size = colored_triangle_frag_size / 4,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .polygon_mode = VK_POLYGON_MODE_FILL,
            .cull_mode = VK_CULL_MODE_NONE,
            .front_face = VK_FRONT_FACE_CLOCKWISE,
            .color_attachment_format = g_IntermediateImage.image.format,
            .depth_attachment_format = VK_FORMAT_UNDEFINED,
        };
        EXPECT(graphics_pipeline_create(&mesh_pipeline_info, &g_MeshPipeline));

        Vertex vertices[4] = {
            {.position = {0.5f, -0.5f, 0.0f}, .color = {0.0f, 0.0f, 0.0f, 1.0f}},
            {.position = {0.5f, 0.5f, 0.0f}, .color = {0.5f, 0.5f, 0.5f, 1.0f}},
            {.position = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
            {.position = {-0.5f, 0.5f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
        };
        uint32_t indices[6] = {0, 1, 2, 2, 1, 3};
        Mesh rectangle = {
            .vertices = vertices,
            .num_vertices = 4,
            .indices = indices,
            .num_indices = 6,
        };
        mesh_buffer_create(&rectangle, &g_RectangleMesh);

        LoadFromFile("../assets/objs/stone-golem.obj");

        return true;
}

void RendererShutdown() {
        // Make sure the GPU has finished all work
        vkDeviceWaitIdle(g_Device);

        compute_pipeline_destroy(&g_GradientPipeline);
        graphics_pipeline_destroy(&g_TrianglePipeline);
        graphics_pipeline_destroy(&g_MeshPipeline);

        descriptor_allocator_destroy(&g_DescriptorAllocator);
        vkDestroyDescriptorSetLayout(g_Device, g_IntermediateImageDescriptorLayout, NULL);
        vkDestroyDescriptorSetLayout(g_Device, g_GlobalDescriptorLayout, NULL);

        allocated_image_destroy(&g_IntermediateImage);
        mesh_buffer_destroy(&g_RectangleMesh);

        swapchain_destroy();
        immediate_command_destroy(&g_ImmediateCommand);

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
        VkRect2D scissor = {
            {0, 0},
            {.width = image->extent.width, .height = image->extent.height},
        };
        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = scissor,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
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

void DrawCommandSetCameraData(CameraData *camera) {
        if (!g_ActiveGraphicsPipeline) {
                printf("No currently bound graphics pipeline!");
                return;
        }
        vmaCopyMemoryToAllocation(g_Allocator, camera,
                                  g_CurrentFrameResources->camera_uniform.allocation, 0,
                                  sizeof(CameraData));

        vkCmdBindDescriptorSets(g_CurrentFrameResources->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_ActiveGraphicsPipeline->layout, 0, 1,
                                &g_CurrentFrameResources->global_descriptors, 0, NULL);
}

void DrawCommandSetGraphicsPushConstants(size_t offset, size_t size, void *data) {
        vkCmdPushConstants(g_CurrentFrameResources->command, g_MeshPipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT, offset, size, data);
}

void DrawCommandBindIndexBuffer(MeshBuffer *mesh) {
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
        vkQueuePresentKHR(g_GraphicsQueue, &present_info);

        // Refresh the window
        SDL_UpdateWindowSurface(g_Window);
}

void RendererDraw() {
        if (!DrawCommandBeginFrame()) {
                return;
        }
        DrawCommandClear(&g_IntermediateImage.image, (VkClearColorValue){0.0f, 0.0f, 1.0f, 0.0f});

        // compute pipeline
        image_transition(&g_IntermediateImage.image, g_CurrentFrameResources->command,
                         VK_IMAGE_LAYOUT_GENERAL);
        DrawCommandBindCompute(&g_GradientPipeline);
        DrawCommandBindComputeDescriptor(&g_GradientPipeline, g_IntermediateImageDescriptors, 0);

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

        DrawCommandBeginRendering(&g_IntermediateImage.image);
        DrawCommandBindGraphicsPipeline(&g_TrianglePipeline);

        vkCmdDraw(g_CurrentFrameResources->command, 3, 1, 0, 0);

        // mesh pipeline
        DrawCommandBindGraphicsPipeline(&g_MeshPipeline);

        mat4 view = mat4_look_at(g_CameraPosition,
                                 vec3_add(g_CameraPosition, g_CameraViewDirection), g_UpVector);
        mat4 proj = mat4_perspective(to_radians(45.0f), (float)g_Width / g_Height, 0.1f, 100.0f);
        mat4 viewproj = mat4_mult(proj, view);

        CameraData camera = {.view = view, .proj = view, .viewproj = viewproj};
        DrawCommandSetCameraData(&camera);

        MeshPushConstants push_constants = {
            .matrix = MAT4_IDENTITY,
            .vertex_address = g_RectangleMesh.vertex_address,
        };
        DrawCommandSetGraphicsPushConstants(0, sizeof(MeshPushConstants), &push_constants);
        DrawCommandBindIndexBuffer(&g_RectangleMesh);

        vkCmdDrawIndexed(g_CurrentFrameResources->command, 6, 1, 0, 0, 0);

        vkCmdEndRendering(g_CurrentFrameResources->command);

        DrawCommandCopyToSwapchain(&g_IntermediateImage.image);
        DrawCommandEndFrame();
}

void MoveCamera(vec3 delta) {
        // TODO: relative movement should be delta rotated by g_CameraViewDirection
        vec3 relative = {delta.x, delta.y, -delta.z};
        g_CameraPosition = vec3_add(g_CameraPosition, relative);
}

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        printf("[Validation]: %s\n", pCallbackData->pMessage);

        return VK_FALSE;
}

bool init_descriptors() {
        EXPECT(descriptor_allocator_create(&g_DescriptorAllocator));

        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            },
        };
        EXPECT(create_descriptor_layout(bindings, sizeof(bindings) / sizeof(*bindings),
                                        &g_IntermediateImageDescriptorLayout));
        EXPECT(descriptor_allocator_allocate(&g_DescriptorAllocator,
                                             &g_IntermediateImageDescriptorLayout, 1,
                                             &g_IntermediateImageDescriptors));

        VkDescriptorSetLayoutBinding global_descriptor_bindings[] = {{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        }};
        EXPECT(create_descriptor_layout(global_descriptor_bindings, 1, &g_GlobalDescriptorLayout));

        VkDescriptorImageInfo image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .imageView = g_IntermediateImage.image.image_view,
        };

        VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .dstSet = g_IntermediateImageDescriptors,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &image_info,
        };

        vkUpdateDescriptorSets(g_Device, 1, &write_set, 0, NULL);

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
            !f13.synchronization2) {
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
        };
        VkPhysicalDeviceVulkan13Features f13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .dynamicRendering = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .pNext = &f12,
        };
        VkPhysicalDeviceFeatures2 f = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
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

bool create_shader_module(const uint32_t *bytes, size_t len, VkShaderModule *module) {
        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pCode = bytes,
            .codeSize = len * sizeof(uint32_t),
        };

        VK_EXPECT(vkCreateShaderModule(g_Device, &create_info, NULL, module));
        return true;
}

bool create_descriptor_pool(VkDescriptorPoolSize *sizes, uint32_t count, VkDescriptorPool *pool) {
        uint32_t sum = 0;
        for (uint32_t i = 0; i < count; i += 1) {
                sum += sizes[i].descriptorCount;
        }

        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pPoolSizes = sizes,
            .poolSizeCount = count,
            .maxSets = sum,
        };

        VK_EXPECT(vkCreateDescriptorPool(g_Device, &create_info, NULL, pool));
        return true;
}

bool create_descriptor_layout(VkDescriptorSetLayoutBinding *bindings, uint32_t count,
                              VkDescriptorSetLayout *layout) {
        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings = bindings,
            .bindingCount = count,
        };

        VK_EXPECT(vkCreateDescriptorSetLayout(g_Device, &create_info, NULL, layout));
        return true;
}
