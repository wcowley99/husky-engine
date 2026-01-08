#pragma once

#include "vkb.h"

#include "command.h"

#include <stdbool.h>

typedef struct {
        VkExtent3D extent;
        VkFormat format;
        VkImageAspectFlags aspect_flags;
        VkImageLayout layout;
        VkImage image;

        VkDevice device;
} ImageCreateInfo;

typedef struct {
        VkImage image;
        VkImageView image_view;
        VkExtent3D extent;
        VkFormat format;
        VkImageLayout layout;
} Image;

bool image_create(ImageCreateInfo *info, Image *image);
void image_destroy(Image *image, VkDevice device);

void image_transition(Image *image, VkCommandBuffer command, VkImageLayout layout);
void image_blit(VkCommandBuffer command, Image *src, Image *dst);
void image_buffer_copy(Image *dst, VkCommandBuffer cmd, VkBuffer buffer, VkExtent3D extent,
                       VkImageAspectFlags flags);

typedef struct {
        VkExtent3D extent;
        VkFormat format;
        VkImageUsageFlags usage_flags;
        VmaMemoryUsage memory_usage;
        VkMemoryPropertyFlags memory_props;
        VkImageAspectFlags aspect_flags;

        void *data;
        ImmediateCommand *imm;

        VmaAllocator allocator;
        VkDevice device;
} AllocatedImageCreateInfo;

typedef struct {
        Image image;
        VmaAllocation allocation;

        VmaAllocator allocator;
} AllocatedImage;

bool allocated_image_create(AllocatedImageCreateInfo *info, AllocatedImage *image);

void allocated_image_destroy(AllocatedImage *image, VkDevice device);
