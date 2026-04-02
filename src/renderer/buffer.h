#pragma once

#include "vkb.h"

#include <stdbool.h>

typedef struct {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;

        VmaAllocator allocator;

        int mapped;
} Buffer;

void buffer_create(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage usage, Buffer *buffer);
void buffer_destroy(Buffer *buffer);

void *buffer_mmap(Buffer *buffer);
void buffer_munmap(Buffer *buffer);

void vk_memory_allocator_init();
void vk_memory_allocator_shutdown();
VmaAllocator vk_memory_allocator();
