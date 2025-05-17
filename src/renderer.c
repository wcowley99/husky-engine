#include "renderer.h"

#include "gradient2_comp.h"
#include "gradient_comp.h"

#include <SDL3/SDL_vulkan.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////
/// Image
///////////////////////////////////////

bool image_create(VkDevice device, VkImage vk_image, VkExtent2D extent, VkFormat format,
                  VkImageLayout layout, Image *image) {
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

  VK_EXPECT(vkCreateImageView(device, &create_info, NULL, &image->image_view));
  return true;
}

void image_destroy(Image *image, VkDevice device) {
  vkDestroyImageView(device, image->image_view, NULL);
}

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
      .srcOffsets = {{}, {.x = src->extent.width, .y = src->extent.height, .z = src->extent.depth}},
      .dstOffsets = {{}, {.x = dst->extent.width, .y = dst->extent.height, .z = dst->extent.depth}},
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

bool allocated_image_create(VkDevice device, VmaAllocator allocator, AllocatedImageCreateInfo *info,
                            AllocatedImage *image) {
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
  VK_EXPECT(
      vmaCreateImage(allocator, &create_info, &alloc_info, &raw_image, &image->allocation, NULL));

  VkExtent2D extent = {
      .width = info->extent.width,
      .height = info->extent.height,
  };
  EXPECT(image_create(device, raw_image, extent, info->format, VK_IMAGE_LAYOUT_UNDEFINED,
                      &image->image));

  return true;
}

void allocated_image_destroy(AllocatedImage *image, VkDevice device, VmaAllocator allocator) {
  image_destroy(&image->image, device);
  vmaDestroyImage(allocator, image->image.image, image->allocation);
}

///////////////////////////////////////
/// FrameResources
///////////////////////////////////////

bool frame_resources_create(VkDevice device, uint32_t queue_family_index, FrameResources *f) {
  VkCommandPoolCreateInfo command_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queue_family_index,
  };
  VK_EXPECT(vkCreateCommandPool(device, &command_pool_info, NULL, &f->pool));

  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = f->pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VK_EXPECT(vkAllocateCommandBuffers(device, &alloc_info, &f->command));

  VkFenceCreateInfo fence_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VK_EXPECT(vkCreateFence(device, &fence_info, NULL, &f->render_fence));

  VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = 0,
  };
  VK_EXPECT(vkCreateSemaphore(device, &semaphore_info, NULL, &f->swapchain_semaphore));
  VK_EXPECT(vkCreateSemaphore(device, &semaphore_info, NULL, &f->render_semaphore));

  return true;
}

void frame_resources_destroy(FrameResources *f, VkDevice device) {
  vkDestroySemaphore(device, f->swapchain_semaphore, NULL);
  vkDestroySemaphore(device, f->render_semaphore, NULL);

  vkDestroyFence(device, f->render_fence, NULL);

  vkDestroyCommandPool(device, f->pool, NULL);
}

bool frame_resources_submit(FrameResources *f, VkQueue graphics_queue) {
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

  VK_EXPECT(vkQueueSubmit2(graphics_queue, 1, &submit_info, f->render_fence));
  return true;
}

///////////////////////////////////////
/// Compute Pipeline
///////////////////////////////////////

bool compute_pipeline_create(ComputePipeline *p, ComputePipelineInfo *info, VkDevice device) {
  VkPushConstantRange *ranges = malloc(info->num_push_constant_sizes * sizeof(VkPushConstantRange));

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

  VkResult result = vkCreatePipelineLayout(device, &layout_create_info, NULL, &p->layout);
  free(ranges);
  VK_EXPECT(result);

  VkShaderModule compute;
  EXPECT(vk_create_shader_module(&compute, info->shader_source, info->shader_source_size, device));

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

  result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &p->pipeline);
  vkDestroyShaderModule(device, compute, NULL);
  VK_EXPECT(result);
  return true;
}

void compute_pipeline_destroy(ComputePipeline *p, VkDevice device) {
  vkDestroyPipelineLayout(device, p->layout, NULL);
  vkDestroyPipeline(device, p->pipeline, NULL);
}

///////////////////////////////////////
/// Swapchain
///////////////////////////////////////
bool swapchain_create(VkDevice device, VkPhysicalDevice gpu, VkSurfaceKHR surface,
                      uint32_t queue_family_index, Swapchain *sc) {
  sc->format = VK_FORMAT_B8G8R8A8_SRGB;

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

  VkSwapchainCreateInfoKHR create_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .minImageCount = capabilities.minImageCount + 1,
      .imageFormat = sc->format,
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

  VK_EXPECT(vkCreateSwapchainKHR(device, &create_info, NULL, &sc->swapchain));

  vkGetSwapchainImagesKHR(device, sc->swapchain, &sc->image_count, NULL);

  VkImage *images = malloc(sizeof(VkImage) * sc->image_count);
  vkGetSwapchainImagesKHR(device, sc->swapchain, &sc->image_count, images);

  sc->images = malloc(sizeof(Image) * sc->image_count);
  sc->frames = malloc(sizeof(FrameResources) * sc->image_count);
  for (uint32_t i = 0; i < sc->image_count; i += 1) {
    EXPECT(image_create(device, images[i], capabilities.currentExtent, sc->format,
                        VK_IMAGE_LAYOUT_UNDEFINED, &sc->images[i]));
    EXPECT(frame_resources_create(device, queue_family_index, &sc->frames[i]));
  }

  return true;
}

void swapchain_destroy(Swapchain *sc, VkDevice device) {
  for (uint32_t i = 0; i < sc->image_count; i += 1) {
    image_destroy(&sc->images[i], device);
    frame_resources_destroy(&sc->frames[i], device);
  }
  free(sc->images);
  free(sc->frames);
  vkDestroySwapchainKHR(device, sc->swapchain, NULL);
}

bool swapchain_next_frame(Swapchain *sc, VkDevice device, FrameResources **frame, Image **image,
                          uint32_t *image_index) {
  FrameResources *next_frame = &sc->frames[sc->frame_index];
  // todo : is it ok to expect on vkWaitForFences here? If function RV
  // represents whether or not to recreate swapchain, shouls you recreate if
  // fence fails (times out) or quit?
  VK_EXPECT(vkWaitForFences(device, 1, &next_frame->render_fence, VK_TRUE, 1000000000));

  VK_EXPECT(vkAcquireNextImageKHR(device, sc->swapchain, 1000000000,
                                  next_frame->swapchain_semaphore, VK_NULL_HANDLE, image_index));

  sc->frame_index = (sc->frame_index + 1) % sc->image_count;
  *frame = next_frame;
  *image = &sc->images[*image_index];

  return true;
}

///////////////////////////////////////
/// Descriptor Allocator
///////////////////////////////////////

bool descriptor_allocator_create(DescriptorAllocator *allocator, VkDevice device) {
  VkDescriptorPoolSize pool_sizes[] = {
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1}};
  EXPECT(vk_descriptor_pool(&allocator->pool, device, pool_sizes,
                            sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize)));

  return true;
}

void descriptor_allocator_destroy(DescriptorAllocator *allocator, VkDevice device) {
  vkDestroyDescriptorPool(device, allocator->pool, NULL);
}

void descriptor_allocator_clear(DescriptorAllocator *allocator, VkDevice device) {
  vkResetDescriptorPool(device, allocator->pool, 0);
}

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDevice device,
                                   VkDescriptorSetLayout *layouts, uint32_t count,
                                   VkDescriptorSet *sets) {
  VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = allocator->pool,
      .pSetLayouts = layouts,
      .descriptorSetCount = count,
  };

  VK_EXPECT(vkAllocateDescriptorSets(device, &alloc_info, sets));
  return true;
}

///////////////////////////////////////
/// Renderer
///////////////////////////////////////
bool renderer_init(Renderer *r, RendererCreateInfo *c) {
  VK_EXPECT(volkInitialize());

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    printf("Failed to initialize SDL: %s\n", SDL_GetError());
    return false;
  }

  r->window =
      SDL_CreateWindow(c->title, c->width, c->height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
  if (!r->window) {
    printf("Failed to create Window: %s\n", SDL_GetError());
    return false;
  }

  EXPECT(vk_create_instance(&r->instance, c));
  EXPECT(vk_create_debug_messenger(r->instance, &r->debug_messenger));
  EXPECT(vk_create_surface(r->instance, r->window, &r->surface));

  EXPECT(vk_select_gpu(r->instance, r->surface, &r->gpu, &r->queue_family_index));
  EXPECT(
      vk_create_device(r->instance, r->gpu, r->queue_family_index, &r->device, &r->graphics_queue));

  EXPECT(swapchain_create(r->device, r->gpu, r->surface, r->queue_family_index, &r->swapchain));

  EXPECT(vma_allocator(&r->allocator, r->instance, r->gpu, r->device));

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

  EXPECT(allocated_image_create(r->device, r->allocator, &allocated_image_info, &r->draw_image));

  EXPECT(descriptor_allocator_create(&r->global_descriptor_allocator, r->device));

  VkDescriptorSetLayoutBinding bindings[] = {
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
  };
  EXPECT(vk_descriptor_layout(&r->draw_image_descriptor_layout, r->device, bindings,
                              sizeof(bindings) / sizeof(*bindings)));
  EXPECT(descriptor_allocator_allocate(&r->global_descriptor_allocator, r->device,
                                       &r->draw_image_descriptor_layout, 1,
                                       &r->draw_image_descriptors));

  VkDescriptorImageInfo image_info = {
      .sampler = VK_NULL_HANDLE,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .imageView = r->draw_image.image.image_view,
  };

  VkWriteDescriptorSet write_set = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstBinding = 0,
      .dstSet = r->draw_image_descriptors,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = &image_info,
  };

  vkUpdateDescriptorSets(r->device, 1, &write_set, 0, NULL);

  uint32_t sizes[] = {sizeof(float) * 16};
  ComputePipelineInfo pipeline_info = {
      .descriptors = &r->draw_image_descriptor_layout,
      .num_descriptors = 1,
      .push_constant_sizes = sizes,
      .num_push_constant_sizes = 1,
      .shader_source = (const uint32_t *)gradient2_comp_file,
      .shader_source_size = gradient2_comp_size / 4,
  };
  EXPECT(compute_pipeline_create(&r->gradient_pipeline, &pipeline_info, r->device));

  return true;
}

void renderer_shutdown(Renderer *r) {
  // Make sure the GPU has finished all work
  vkDeviceWaitIdle(r->device);

  compute_pipeline_destroy(&r->gradient_pipeline, r->device);

  descriptor_allocator_destroy(&r->global_descriptor_allocator, r->device);
  vkDestroyDescriptorSetLayout(r->device, r->draw_image_descriptor_layout, NULL);

  allocated_image_destroy(&r->draw_image, r->device, r->allocator);
  vmaDestroyAllocator(r->allocator);

  swapchain_destroy(&r->swapchain, r->device);

  vkDestroyDevice(r->device, NULL);

  vkDestroySurfaceKHR(r->instance, r->surface, NULL);
  vkDestroyDebugUtilsMessengerEXT(r->instance, r->debug_messenger, NULL);
  vkDestroyInstance(r->instance, NULL);

  SDL_DestroyWindow(r->window);
  SDL_Quit();
}

void renderer_draw(Renderer *r) {
  FrameResources *frame;
  Image *image;
  uint32_t image_index;

  Image *draw_image = &r->draw_image.image;

  if (!swapchain_next_frame(&r->swapchain, r->device, &frame, &image, &image_index)) {
    vkDeviceWaitIdle(r->device);

    swapchain_destroy(&r->swapchain, r->device);
    swapchain_create(r->device, r->gpu, r->surface, r->queue_family_index, &r->swapchain);

    return;
  }

  vkResetFences(r->device, 1, &frame->render_fence);

  vk_begin_command_buffer(frame->command);

  image_transition(draw_image, frame->command, VK_IMAGE_LAYOUT_GENERAL);
  VkClearColorValue color = {{0.0f, 1.0f, 0.0f, 1.0f}};
  VkImageSubresourceRange range = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };
  vkCmdClearColorImage(frame->command, draw_image->image, draw_image->layout, &color, 1, &range);

  image_transition(draw_image, frame->command, VK_IMAGE_LAYOUT_GENERAL);
  vkCmdBindPipeline(frame->command, VK_PIPELINE_BIND_POINT_COMPUTE, r->gradient_pipeline.pipeline);

  vkCmdBindDescriptorSets(frame->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r->gradient_pipeline.layout, 0, 1, &r->draw_image_descriptors, 0, NULL);

  struct {
    vec4 bottom;
    vec4 top;
    float padding[8];
  } pc = {
      .bottom = {1.0f, 0.0f, 0.0f, 1.0f},
      .top = {0.0f, 0.0f, 1.0f, 1.0f},
  };
  vkCmdPushConstants(frame->command, r->gradient_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(pc), &pc);

  vkCmdDispatch(frame->command, ceilf(draw_image->extent.width / 16.0f),
                ceilf(draw_image->extent.height / 16.0f), 1);

  image_transition(draw_image, frame->command, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  image_transition(image, frame->command, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  image_blit(frame->command, draw_image, image);

  image_transition(image, frame->command, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  vkEndCommandBuffer(frame->command);

  frame_resources_submit(frame, r->graphics_queue);

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .swapchainCount = 1,
      .pSwapchains = &r->swapchain.swapchain,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame->render_semaphore,
      .pImageIndices = &image_index,
  };
  vkQueuePresentKHR(r->graphics_queue, &present_info);

  // Refresh the window
  SDL_UpdateWindowSurface(r->window);
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

bool vma_allocator(VmaAllocator *allocator, VkInstance instance, VkPhysicalDevice gpu,
                   VkDevice device) {
  VmaVulkanFunctions functions = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
  };

  VmaAllocatorCreateInfo create_info = {
      .physicalDevice = gpu,
      .instance = instance,
      .device = device,
      .vulkanApiVersion = VK_API_VERSION_1_3,
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .pVulkanFunctions = &functions,
  };

  VK_EXPECT(vmaCreateAllocator(&create_info, allocator));
  return true;
}

bool vk_create_instance(VkInstance *instance, RendererCreateInfo *c) {
  VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                .pApplicationName = c->title,
                                .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
                                .pEngineName = "No Engine",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};

  VkDebugUtilsMessengerCreateInfoEXT validation_callback = {};
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

  VK_EXPECT(vkCreateInstance(&create_info, NULL, instance));
  volkLoadInstance(*instance);

  free(extensions);
  return true;
}

bool vk_create_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT *debug_messenger) {
  VkDebugUtilsMessengerCreateInfoEXT create_info = {};
  vk_debug_messenger_create_info(&create_info);

  VK_EXPECT(vkCreateDebugUtilsMessengerEXT(instance, &create_info, NULL, debug_messenger));
  return true;
}

bool vk_create_surface(VkInstance instance, SDL_Window *window, VkSurfaceKHR *surface) {
  if (!SDL_Vulkan_CreateSurface(window, instance, NULL, surface)) {
    printf("SDL failed to create window: %s.\n", SDL_GetError());
    return false;
  }

  return true;
}

bool vk_gpu_suitable(VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *queue_family_index) {
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
    VK_EXPECT(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &support));

    if (support && properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      *queue_family_index = i;
      printf("Selected Device: %s.\n", p.deviceName);
    }
  }

  free(properties);
  return true;
}

bool vk_select_gpu(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice *gpu,
                   uint32_t *queue_family_index) {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, NULL);

  if (count == 0) {
    printf("Failed to find GPUs with Vulkan support.\n");
    return false;
  }

  VkPhysicalDevice *gpus = malloc(sizeof(VkPhysicalDevice *) * count);
  vkEnumeratePhysicalDevices(instance, &count, gpus);

  for (uint32_t i = 0; i < count; i += 1) {
    if (vk_gpu_suitable(gpus[i], surface, queue_family_index)) {
      *gpu = gpus[i];
      break;
    }
  }

  free(gpus);
  return true;
}

bool vk_create_device(VkInstance instance, VkPhysicalDevice gpu, uint32_t queue_family_index,
                      VkDevice *device, VkQueue *graphics_queue) {
  float priorities[] = {1.0f};
  VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                        .queueFamilyIndex = queue_family_index,
                                        .queueCount = 1,
                                        .pQueuePriorities = priorities};

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

  VK_EXPECT(vkCreateDevice(gpu, &create_info, NULL, device));
  volkLoadDevice(*device);

  vkGetDeviceQueue(*device, queue_family_index, 0, graphics_queue);

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

bool vk_create_shader_module(VkShaderModule *module, const uint32_t *bytes, size_t len,
                             VkDevice device) {
  VkShaderModuleCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pCode = bytes,
      .codeSize = len * sizeof(uint32_t),
  };

  VK_EXPECT(vkCreateShaderModule(device, &create_info, NULL, module));
  return true;
}

bool vk_descriptor_pool(VkDescriptorPool *pool, VkDevice device, VkDescriptorPoolSize *sizes,
                        uint32_t count) {
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

  VK_EXPECT(vkCreateDescriptorPool(device, &create_info, NULL, pool));
  return true;
}

bool vk_descriptor_layout(VkDescriptorSetLayout *layout, VkDevice device,
                          VkDescriptorSetLayoutBinding *bindings, uint32_t count) {
  VkDescriptorSetLayoutCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pBindings = bindings,
      .bindingCount = count,
  };

  VK_EXPECT(vkCreateDescriptorSetLayout(device, &create_info, NULL, layout));
  return true;
}
