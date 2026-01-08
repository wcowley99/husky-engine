#include "image.h"

#include "buffer.h"

#include <assert.h>

bool image_create(ImageCreateInfo *info, Image *image) {
        image->image = info->image;

        image->extent.width = info->extent.width;
        image->extent.height = info->extent.height;
        image->extent.depth = 1;

        image->format = info->format;
        image->layout = info->layout;

        VkComponentMapping mapping = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        };

        VkImageSubresourceRange range = {
            .aspectMask = info->aspect_flags,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .baseMipLevel = 0,
            .levelCount = 1,
        };

        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = info->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = info->format,
            .components = mapping,
            .subresourceRange = range,
        };

        VK_EXPECT(vkCreateImageView(info->device, &create_info, NULL, &image->image_view));
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

void image_buffer_copy(Image *dst, VkCommandBuffer cmd, VkBuffer buffer, VkExtent3D extent,
                       VkImageAspectFlags flags) {
        image_transition(dst, cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy copy_region = {
            .bufferOffset = 0,
            .bufferImageHeight = 0,
            .bufferRowLength = 0,
            .imageSubresource.aspectMask = flags,
            .imageSubresource.mipLevel = 0,
            .imageSubresource.baseArrayLayer = 0,
            .imageSubresource.layerCount = 1,
            .imageExtent = extent,
        };
        vkCmdCopyBufferToImage(cmd, buffer, dst->image, dst->layout, 1, &copy_region);
}

bool allocated_image_create(AllocatedImageCreateInfo *info, AllocatedImage *image) {
        image->allocator = info->allocator;

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
        VK_EXPECT(vmaCreateImage(info->allocator, &create_info, &alloc_info, &raw_image,
                                 &image->allocation, NULL));

        VkExtent3D extent = {
            .width = info->extent.width,
            .height = info->extent.height,
            .depth = info->extent.depth,
        };

        ImageCreateInfo image_info = {
            .image = raw_image,
            .extent = extent,
            .format = info->format,
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .aspect_flags = info->aspect_flags,

            .device = info->device,
        };

        EXPECT(image_create(&image_info, &image->image));

        if (info->data) {
                assert(info->imm);

                size_t size = info->extent.width * info->extent.height * info->extent.depth * 4;
                Buffer staging_buffer;
                buffer_create(info->allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VMA_MEMORY_USAGE_CPU_ONLY, &staging_buffer);
                vmaCopyMemoryToAllocation(info->allocator, info->data, staging_buffer.allocation, 0,
                                          size);

                VkCommandBuffer cmd = info->imm->command;
                immediate_command_begin(info->imm);

                image_buffer_copy(&image->image, cmd, staging_buffer.buffer, extent,
                                  info->aspect_flags);

                image_transition(&image->image, cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                immediate_command_end(info->imm);

                buffer_destroy(&staging_buffer);
        }

        return true;
}

void allocated_image_destroy(AllocatedImage *image, VkDevice device) {
        image_destroy(&image->image, device);
        vmaDestroyImage(image->allocator, image->image.image, image->allocation);
}
