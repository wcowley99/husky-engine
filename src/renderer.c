#include "renderer.h"

#include "colored_triangle_frag.h"
#include "colored_triangle_mesh_vert.h"
#include "colored_triangle_vert.h"
#include "gradient2_comp.h"
#include "gradient_comp.h"

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

Swapchain g_Swapchain;

VmaAllocator g_Allocator;
ImmediateCommand g_ImmediateCommand;

DescriptorAllocator g_DescriptorAllocator;

AllocatedImage g_IntermediateImage;
VkDescriptorSetLayout g_IntermediateImageDescriptorLayout;
VkDescriptorSet g_IntermediateImageDescriptors;

ComputePipeline g_GradientPipeline;
GraphicsPipeline g_TrianglePipeline;
GraphicsPipeline g_MeshPipeline;

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

        return true;
}

void frame_resources_destroy(FrameResources *f) {
        vkDestroySemaphore(g_Device, f->swapchain_semaphore, NULL);
        vkDestroySemaphore(g_Device, f->render_semaphore, NULL);

        vkDestroyFence(g_Device, f->render_fence, NULL);

        vkDestroyCommandPool(g_Device, f->pool, NULL);
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
        EXPECT(vk_create_shader_module(&compute, info->shader_source, info->shader_source_size));

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
        EXPECT(vk_create_shader_module(&vertex, create_info->vertex_shader,
                                       create_info->vertex_shader_size));
        EXPECT(vk_create_shader_module(&fragment, create_info->fragment_shader,
                                       create_info->fragment_shader_size));

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
        g_Swapchain.format = VK_FORMAT_B8G8R8A8_SRGB;

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);

        VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = g_Surface,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .minImageCount = capabilities.minImageCount + 1,
            .imageFormat = g_Swapchain.format,
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

        VK_EXPECT(vkCreateSwapchainKHR(g_Device, &create_info, NULL, &g_Swapchain.swapchain));

        vkGetSwapchainImagesKHR(g_Device, g_Swapchain.swapchain, &g_Swapchain.image_count, NULL);

        VkImage *images = malloc(sizeof(VkImage) * g_Swapchain.image_count);
        vkGetSwapchainImagesKHR(g_Device, g_Swapchain.swapchain, &g_Swapchain.image_count, images);

        g_Swapchain.images = malloc(sizeof(Image) * g_Swapchain.image_count);
        g_Swapchain.frames = malloc(sizeof(FrameResources) * g_Swapchain.image_count);
        for (uint32_t i = 0; i < g_Swapchain.image_count; i += 1) {
                EXPECT(image_create(images[i], capabilities.currentExtent, g_Swapchain.format,
                                    VK_IMAGE_LAYOUT_UNDEFINED, &g_Swapchain.images[i]));
                EXPECT(frame_resources_create(&g_Swapchain.frames[i]));
        }

        return true;
}

void swapchain_destroy() {
        for (uint32_t i = 0; i < g_Swapchain.image_count; i += 1) {
                image_destroy(&g_Swapchain.images[i]);
                frame_resources_destroy(&g_Swapchain.frames[i]);
        }
        free(g_Swapchain.images);
        free(g_Swapchain.frames);
        vkDestroySwapchainKHR(g_Device, g_Swapchain.swapchain, NULL);
}

bool swapchain_next_frame(FrameResources **frame, Image **image, uint32_t *image_index) {
        FrameResources *next_frame = &g_Swapchain.frames[g_Swapchain.frame_index];
        // todo : is it ok to expect on vkWaitForFences here? If function RV
        // represents whether or not to recreate swapchain, shouls you recreate if
        // fence fails (times out) or quit?
        VK_EXPECT(vkWaitForFences(g_Device, 1, &next_frame->render_fence, VK_TRUE, 1000000000));

        VK_EXPECT(vkAcquireNextImageKHR(g_Device, g_Swapchain.swapchain, 1000000000,
                                        next_frame->swapchain_semaphore, VK_NULL_HANDLE,
                                        image_index));

        g_Swapchain.frame_index = (g_Swapchain.frame_index + 1) % g_Swapchain.image_count;
        *frame = next_frame;
        *image = &g_Swapchain.images[*image_index];

        return true;
}

///////////////////////////////////////
/// Descriptor Allocator
///////////////////////////////////////

bool descriptor_allocator_create(DescriptorAllocator *allocator) {
        VkDescriptorPoolSize pool_sizes[] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1}};
        EXPECT(vk_descriptor_pool(&allocator->pool, pool_sizes,
                                  sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize)));

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

        EXPECT(vk_create_instance(c));
        EXPECT(vk_create_debug_messenger());
        EXPECT(vk_create_surface());

        EXPECT(vk_select_gpu());
        EXPECT(vk_create_device());

        EXPECT(swapchain_create());

        EXPECT(vma_allocator());

        EXPECT(immediate_command_create(&g_ImmediateCommand));

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

        EXPECT(descriptor_allocator_create(&g_DescriptorAllocator));

        VkDescriptorSetLayoutBinding bindings[] = {
            {.binding = 0,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .descriptorCount = 1,
             .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        };
        EXPECT(vk_descriptor_layout(&g_IntermediateImageDescriptorLayout, bindings,
                                    sizeof(bindings) / sizeof(*bindings)));
        EXPECT(descriptor_allocator_allocate(&g_DescriptorAllocator,
                                             &g_IntermediateImageDescriptorLayout, 1,
                                             &g_IntermediateImageDescriptors));

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
            {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 64},
        };
        GraphicsPipelineCreateInfo mesh_pipeline_info = {
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

        allocated_image_destroy(&g_IntermediateImage);

        vmaDestroyAllocator(g_Allocator);
        immediate_command_destroy(&g_ImmediateCommand);

        swapchain_destroy();

        vkDestroyDevice(g_Device, NULL);

        vkDestroySurfaceKHR(g_Instance, g_Surface, NULL);
        vkDestroyDebugUtilsMessengerEXT(g_Instance, g_DebugMessenger, NULL);
        vkDestroyInstance(g_Instance, NULL);

        SDL_DestroyWindow(g_Window);
        SDL_Quit();
}

void RendererDraw() {
        FrameResources *frame;
        Image *image;
        uint32_t image_index;

        Image *intermediate_image = &g_IntermediateImage.image;

        if (!swapchain_next_frame(&frame, &image, &image_index)) {
                vkDeviceWaitIdle(g_Device);

                swapchain_destroy();
                swapchain_create();

                return;
        }

        vkResetFences(g_Device, 1, &frame->render_fence);

        vk_begin_command_buffer(frame->command);

        image_transition(intermediate_image, frame->command, VK_IMAGE_LAYOUT_GENERAL);
        VkClearColorValue color = {{0.0f, 1.0f, 0.0f, 1.0f}};
        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };
        vkCmdClearColorImage(frame->command, intermediate_image->image, intermediate_image->layout,
                             &color, 1, &range);

        // compute pipeline
        image_transition(intermediate_image, frame->command, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBindPipeline(frame->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          g_GradientPipeline.pipeline);

        vkCmdBindDescriptorSets(frame->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                g_GradientPipeline.layout, 0, 1, &g_IntermediateImageDescriptors, 0,
                                NULL);

        struct {
                vec4 bottom;
                vec4 top;
                float padding[8];
        } pc = {
            .bottom = {1.0f, 0.0f, 0.0f, 1.0f},
            .top = {0.0f, 0.0f, 1.0f, 1.0f},
        };
        vkCmdPushConstants(frame->command, g_GradientPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(pc), &pc);

        vkCmdDispatch(frame->command, ceilf(intermediate_image->extent.width / 16.0f),
                      ceilf(intermediate_image->extent.height / 16.0f), 1);

        // graphics pipeline
        image_transition(intermediate_image, frame->command,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = intermediate_image->image_view,
            .imageLayout = intermediate_image->layout,
        };
        VkRect2D scissor = {
            {0, 0},
            {.width = intermediate_image->extent.width,
             .height = intermediate_image->extent.height},
        };
        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = scissor,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
            .layerCount = 1,
        };

        vkCmdBeginRendering(frame->command, &render_info);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = intermediate_image->extent.width,
            .height = intermediate_image->extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(frame->command, 0, 1, &viewport);
        vkCmdSetScissor(frame->command, 0, 1, &scissor);

        vkCmdBindPipeline(frame->command, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g_TrianglePipeline.pipeline);

        vkCmdDraw(frame->command, 3, 1, 0, 0);

        vkCmdEndRendering(frame->command);

        image_transition(intermediate_image, frame->command, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        image_transition(image, frame->command, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        image_blit(frame->command, intermediate_image, image);

        image_transition(image, frame->command, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(frame->command);

        frame_resources_submit(frame);

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &g_Swapchain.swapchain,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame->render_semaphore,
            .pImageIndices = &image_index,
        };
        vkQueuePresentKHR(g_GraphicsQueue, &present_info);

        // Refresh the window
        SDL_UpdateWindowSurface(g_Window);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        printf("[Validation]: %s\n", pCallbackData->pMessage);

        return VK_FALSE;
}

void vk_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT *info) {
        info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info->pfnUserCallback = vk_validation_callback;
}

bool vma_allocator() {
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

bool vk_create_instance(RendererCreateInfo *c) {
        VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                      .pApplicationName = c->title,
                                      .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
                                      .pEngineName = "No Engine",
                                      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                      .apiVersion = VK_API_VERSION_1_3};

        VkDebugUtilsMessengerCreateInfoEXT validation_callback = {0};
        vk_debug_messenger_create_info(&validation_callback);

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

bool vk_create_debug_messenger() {
        VkDebugUtilsMessengerCreateInfoEXT create_info = {0};
        vk_debug_messenger_create_info(&create_info);

        VK_EXPECT(
            vkCreateDebugUtilsMessengerEXT(g_Instance, &create_info, NULL, &g_DebugMessenger));
        return true;
}

bool vk_create_surface() {
        if (!SDL_Vulkan_CreateSurface(g_Window, g_Instance, NULL, &g_Surface)) {
                printf("SDL failed to create window: %s.\n", SDL_GetError());
                return false;
        }

        return true;
}

bool vk_gpu_suitable(VkPhysicalDevice gpu) {
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

bool vk_select_gpu() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(g_Instance, &count, NULL);

        if (count == 0) {
                printf("Failed to find GPUs with Vulkan support.\n");
                return false;
        }

        VkPhysicalDevice *gpus = malloc(sizeof(VkPhysicalDevice *) * count);
        vkEnumeratePhysicalDevices(g_Instance, &count, gpus);

        for (uint32_t i = 0; i < count; i += 1) {
                if (vk_gpu_suitable(gpus[i])) {
                        g_PhysicalDevice = gpus[i];
                        break;
                }
        }

        free(gpus);
        return true;
}

bool vk_create_device() {
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

bool vk_begin_command_buffer(VkCommandBuffer command) {
        VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_EXPECT(vkResetCommandBuffer(command, 0));
        VK_EXPECT(vkBeginCommandBuffer(command, &info));

        return true;
}

bool vk_create_shader_module(VkShaderModule *module, const uint32_t *bytes, size_t len) {
        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pCode = bytes,
            .codeSize = len * sizeof(uint32_t),
        };

        VK_EXPECT(vkCreateShaderModule(g_Device, &create_info, NULL, module));
        return true;
}

bool vk_descriptor_pool(VkDescriptorPool *pool, VkDescriptorPoolSize *sizes, uint32_t count) {
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

bool vk_descriptor_layout(VkDescriptorSetLayout *layout, VkDescriptorSetLayoutBinding *bindings,
                          uint32_t count) {
        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings = bindings,
            .bindingCount = count,
        };

        VK_EXPECT(vkCreateDescriptorSetLayout(g_Device, &create_info, NULL, layout));
        return true;
}
