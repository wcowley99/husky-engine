#include "descriptors.h"

#include "vkb.h"

bool descriptor_allocator_create(VkDevice device, DescriptorAllocator *allocator) {
        allocator->device = device;
        VkDescriptorPoolSize pool_sizes[] = {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 4},
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 4},
        };
        EXPECT(create_descriptor_pool(device, pool_sizes,
                                      sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
                                      &allocator->pool));

        return true;
}

void descriptor_allocator_destroy(DescriptorAllocator *allocator) {
        vkDestroyDescriptorPool(allocator->device, allocator->pool, NULL);
}

void descriptor_allocator_clear(DescriptorAllocator *allocator) {
        vkResetDescriptorPool(allocator->device, allocator->pool, 0);
}

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDescriptorSetLayout *layouts,
                                   uint32_t count, VkDescriptorSet *sets) {
        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = allocator->pool,
            .pSetLayouts = layouts,
            .descriptorSetCount = count,
        };

        VK_EXPECT(vkAllocateDescriptorSets(allocator->device, &alloc_info, sets));
        return true;
}

bool create_descriptor_pool(VkDevice device, VkDescriptorPoolSize *sizes, uint32_t count,
                            VkDescriptorPool *pool) {
        uint32_t sum = 0;
        for (uint32_t i = 0; i < count; i += 1) {
                sum += sizes[i].descriptorCount;
        }

        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pPoolSizes = sizes,
            .poolSizeCount = count,
            .maxSets = sum,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        };

        VK_EXPECT(vkCreateDescriptorPool(device, &create_info, NULL, pool));
        return true;
}
