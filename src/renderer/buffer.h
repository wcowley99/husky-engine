#pragma once

#include "vkb.h"

#include <stdbool.h>

typedef struct {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;

        VmaAllocator allocator;
} Buffer;

bool buffer_create(VmaAllocator allocator, size_t size, VkBufferUsageFlags flags,
                   VmaMemoryUsage usage, Buffer *buffer);
void buffer_destroy(Buffer *buffer);
