#pragma once

#include "vkb.h"

#include <stdbool.h>

typedef struct {
        VkDescriptorPool pool;

        VkDevice device;
} DescriptorAllocator;

bool descriptor_allocator_create(VkDevice device, DescriptorAllocator *allocator);

void descriptor_allocator_destroy(DescriptorAllocator *allocator);

void descriptor_allocator_clear(DescriptorAllocator *allocator);

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDescriptorSetLayout *layouts,
                                   uint32_t count, VkDescriptorSet *sets);

bool create_descriptor_pool(VkDevice device, VkDescriptorPoolSize *sizes, uint32_t count,
                            VkDescriptorPool *pool);
