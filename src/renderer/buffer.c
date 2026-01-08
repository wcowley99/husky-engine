#include "buffer.h"

bool buffer_create(VmaAllocator allocator, size_t size, VkBufferUsageFlags flags,
                   VmaMemoryUsage usage, Buffer *buffer) {
        buffer->allocator = allocator;
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = flags,
        };

        VmaAllocationCreateInfo alloc_info = {
            .usage = usage,
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        };

        VkResult result = vmaCreateBuffer(allocator, &create_info, &alloc_info, &buffer->buffer,
                                          &buffer->allocation, &buffer->info);

        if (result != VK_SUCCESS) {
                printf("Failed to create buffer.\n");
                return false;
        } else {
                return true;
        }
}

void buffer_destroy(Buffer *buffer) {
        vmaDestroyBuffer(buffer->allocator, buffer->buffer, buffer->allocation);
}
