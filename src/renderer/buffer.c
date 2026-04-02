#include "buffer.h"

#include "vk_context.h"

#include "husky.h"

typedef struct gpu_buffers {

} gpu_buffers_t;

static VmaAllocator g_allocator;

void vk_memory_allocator_init() {
        VmaVulkanFunctions functions = {
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        };

        VmaAllocatorCreateInfo create_info = {
            .physicalDevice = vk_context_physical_device(),
            .instance = vk_context_instance(),
            .device = vk_context_device(),
            .vulkanApiVersion = VK_API_VERSION_1_3,
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .pVulkanFunctions = &functions,
        };

        VK_EXPECT(vmaCreateAllocator(&create_info, &g_allocator));
}

void vk_memory_allocator_shutdown() { vmaDestroyAllocator(g_allocator); }

VmaAllocator vk_memory_allocator() { return g_allocator; }

void buffer_create(size_t size, VkBufferUsageFlags flags, VmaMemoryUsage usage, Buffer *buffer) {
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = flags,
        };

        VmaAllocationCreateInfo alloc_info = {
            .usage = usage,
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        };

        VK_EXPECT(vmaCreateBuffer(g_allocator, &create_info, &alloc_info, &buffer->buffer,
                                  &buffer->allocation, &buffer->info));

        buffer->mapped = 0;
}

void buffer_destroy(Buffer *buffer) {
        vmaDestroyBuffer(vk_memory_allocator(), buffer->buffer, buffer->allocation);
}

void *buffer_mmap(Buffer *buffer) {
        ASSERT(!buffer->mapped);
        buffer->mapped = 1;

        void *ssbo;
        vmaMapMemory(g_allocator, buffer->allocation, &ssbo);

        return ssbo;
}

void buffer_munmap(Buffer *buffer) {
        ASSERT(buffer->mapped);
        buffer->mapped = 0;

        vmaFlushAllocation(g_allocator, buffer->allocation, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(g_allocator, buffer->allocation);
}
