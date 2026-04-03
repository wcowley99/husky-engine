#include "renderer/swapchain.h"

#include "renderer/vk_context.h"

#include "renderer/buffer.h"
#include "renderer/descriptors.h"
#include "renderer/vkb.h"

#include "husky.h"

typedef struct {
        VkCommandPool pool;
        VkCommandBuffer command;
        VkSemaphore swapchain_semaphore;
        VkSemaphore render_semaphore;
        VkFence render_fence;

        Buffer camera_uniform;
        Buffer instance_buffer;

        Descriptor global_descriptors;
        VkDescriptorSet mat_descriptors;
} frame_t;

static frame_t *g_frames;
static uint32_t g_current_frame_index;
static VkSwapchainKHR g_Swapchain;
static uint32_t g_SwapchainImageCount;
static uint32_t g_current_image_index;
static Image *g_SwapchainImages;
const static VkFormat g_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

void frame_resources_init(frame_t *f) {
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

        buffer_create(sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU, &f->camera_uniform);
        buffer_create(sizeof(Instance) * MAX_INSTANCES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, &f->instance_buffer);

        f->global_descriptors = descriptor_allocate(global_descriptor_layout());

        descriptor_write_buffer(f->global_descriptors, &f->camera_uniform, 0, 0);
        descriptor_write_buffer(f->global_descriptors, &f->instance_buffer, 1, 0);
}

void frame_resources_destroy(frame_t *f) {
        vkDestroySemaphore(vk_context_device(), f->swapchain_semaphore, NULL);
        vkDestroySemaphore(vk_context_device(), f->render_semaphore, NULL);

        vkDestroyFence(vk_context_device(), f->render_fence, NULL);

        vkDestroyCommandPool(vk_context_device(), f->pool, NULL);

        descriptor_free(&f->global_descriptors);

        buffer_destroy(&f->camera_uniform);
        buffer_destroy(&f->instance_buffer);
}

void begin_command_buffer(VkCommandBuffer command) {
        VkCommandBufferBeginInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_EXPECT(vkResetCommandBuffer(command, 0));
        VK_EXPECT(vkBeginCommandBuffer(command, &info));
}

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

        VkExtent3D image_extent = {extent.width, extent.height, 1};

        VkImage *images = malloc(sizeof(VkImage) * g_SwapchainImageCount);
        vkGetSwapchainImagesKHR(vk_context_device(), g_Swapchain, &g_SwapchainImageCount, images);

        g_SwapchainImages = malloc(sizeof(Image) * g_SwapchainImageCount);

        g_frames = malloc(sizeof(frame_t) * g_SwapchainImageCount);

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
                frame_resources_init(&g_frames[i]);
        }

        free(images);

        return true;
}

void swapchain_destroy() {
        for (uint32_t i = 0; i < g_SwapchainImageCount; i += 1) {
                image_destroy(&g_SwapchainImages[i], vk_context_device());
                frame_resources_destroy(&g_frames[i]);
        }
        free(g_SwapchainImages);
        free(g_frames);
        vkDestroySwapchainKHR(vk_context_device(), g_Swapchain, NULL);
}

void SwapchainRecreate() {
        vkDeviceWaitIdle(vk_context_device());

        swapchain_destroy();
        swapchain_create();
}

bool swapchain_next_frame() {
        uint32_t next_frame_index = (g_current_frame_index + 1) % g_SwapchainImageCount;
        frame_t *next = &g_frames[next_frame_index];
        // todo : is it ok to expect on vkWaitForFences here? If function RV
        // represents whether or not to recreate swapchain, should you recreate if
        // fence fails (times out) or quit?
        VK_EXPECT(
            vkWaitForFences(vk_context_device(), 1, &next->render_fence, VK_TRUE, 1000000000));
        VK_EXPECT(vkResetFences(vk_context_device(), 1, &next->render_fence));

        VK_EXPECT(vkAcquireNextImageKHR(vk_context_device(), g_Swapchain, 1000000000,
                                        next->swapchain_semaphore, VK_NULL_HANDLE,
                                        &g_current_image_index));

        g_current_frame_index = next_frame_index;

        return true;
}

void swapchain_current_frame_submit() {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        VkCommandBufferSubmitInfo command_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = current_frame->command,
        };

        VkSemaphoreSubmitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = current_frame->swapchain_semaphore,
            .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0,
            .value = 1,
        };

        VkSemaphoreSubmitInfo signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = current_frame->render_semaphore,
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

        VK_EXPECT(vkQueueSubmit2(vk_context_graphics_queue(), 1, &submit_info,
                                 current_frame->render_fence));

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &g_Swapchain,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &current_frame->render_semaphore,
            .pImageIndices = &g_current_image_index,
        };
        VkResult result = vkQueuePresentKHR(vk_context_graphics_queue(), &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                DEBUG("out of date/suboptimal swapchain.");
                SwapchainRecreate();
        }
}

void swapchain_current_frame_begin() {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        begin_command_buffer(swapchain_current_frame_command_buffer());
}

Descriptor *swapchain_current_frame_global_descriptor() {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        return &current_frame->global_descriptors;
}

void *swapchain_current_frame_get_buffer(frame_buffer_type_t buffer_type) {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        switch (buffer_type) {
        case FRAME_BUFFER_CAMERA:
                return buffer_mmap(&current_frame->camera_uniform);
        case FRAME_BUFFER_INSTANCES:
                return buffer_mmap(&current_frame->instance_buffer);
        default:
                DEBUG("error: unknown buffer type %d", buffer_type);
                exit(1);
        }

        return NULL;
}

void swapchain_current_frame_unmap_buffer(frame_buffer_type_t buffer_type) {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        switch (buffer_type) {
        case FRAME_BUFFER_CAMERA:
                buffer_munmap(&current_frame->camera_uniform);
                break;
        case FRAME_BUFFER_INSTANCES:
                buffer_munmap(&current_frame->instance_buffer);
                break;
        default:
                DEBUG("error: unknown buffer type %d", buffer_type);
                exit(1);
        }
}

VkCommandBuffer swapchain_current_frame_command_buffer() {
        frame_t *current_frame = &g_frames[g_current_frame_index];

        return current_frame->command;
}

Image *swapchain_current_image() { return &g_SwapchainImages[g_current_image_index]; }

void swapchain_descriptors_write_texture(Image *image, uint32_t arr_index, VkSampler sampler) {
        for (int i = 0; i < g_SwapchainImageCount; i += 1) {
                Descriptor texture_descriptor = g_frames[i].global_descriptors;
                descriptor_write_texture(texture_descriptor, image, 2, arr_index, sampler);
        }
}
