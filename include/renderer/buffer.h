#pragma once

#include "vkb.h"

#include <stdbool.h>

typedef struct buffer {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo info;

        int mapped;
} buffer_t;

void buffer_create(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage usage, buffer_t *buffer);
void buffer_destroy(buffer_t *buffer);

void *buffer_mmap(buffer_t *buffer);
void buffer_munmap(buffer_t *buffer);

void vk_memory_allocator_init();
void vk_memory_allocator_shutdown();
VmaAllocator vk_memory_allocator();
