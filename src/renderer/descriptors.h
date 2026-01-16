#pragma once

#include "vkb.h"

#include "buffer.h"
#include "image.h"

#include <stdbool.h>

typedef struct {
        VkDescriptorType type;
        VkShaderStageFlags stage;
        uint32_t count;
        bool variable;
} DescriptorBinding;

typedef struct {
        VkDescriptorSetLayout layout;
        VkDescriptorType *binding_types;
        uint32_t *binding_counts;
        bool *variable;

        VkDevice device;
} DescriptorLayout;

DescriptorLayout descriptor_layout_create(VkDevice device, DescriptorBinding *bindings,
                                          uint32_t count);

void descriptor_layout_destroy(DescriptorLayout *layout);

typedef struct {
        VkDescriptorPool pool;

        VkDescriptorPoolSize *pool_sizes;
        uint32_t num_frames;

        VkDevice device;
} DescriptorAllocator;

void descriptor_allocator_init(DescriptorAllocator *allocator, uint32_t num_frames);

void descriptor_allocator_destroy(DescriptorAllocator *allocator);

void descriptor_allocator_clear(DescriptorAllocator *allocator);

void descriptor_allocator_reserve(DescriptorAllocator *allocator, DescriptorBinding *bindings,
                                  uint32_t count, bool per_frame_descriptor);

bool descriptor_allocator_create(VkDevice device, DescriptorAllocator *allocator);

typedef struct {
        DescriptorLayout *layout;
        VkDescriptorSet descriptor;
} Descriptor;

Descriptor descriptor_allocate(DescriptorAllocator *allocator, DescriptorLayout *layout);

void descriptor_write_image(Descriptor descriptor, Image *image, uint32_t binding,
                            uint32_t arr_index);
void descriptor_write_texture(Descriptor descriptor, Image *image, uint32_t binding,
                              uint32_t arr_index, VkSampler sampler);
void descriptor_write_buffer(Descriptor descriptor, Buffer *buffer, uint32_t binding,
                             uint32_t arr_index);
