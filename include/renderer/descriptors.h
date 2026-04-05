#pragma once

#include "buffer.h"
#include "image.h"
#include "vkb.h"

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

void descriptors_init();
void descriptors_shutdown();

void global_descriptor_layout_init();
DescriptorLayout *global_descriptor_layout();

void descriptor_allocator_init(uint32_t num_frames);
void descriptor_allocator_destroy();
void descriptor_allocator_clear();
void descriptor_allocator_reserve(DescriptorBinding *bindings, uint32_t count,
                                  bool per_frame_descriptor);

bool descriptor_allocator_create(VkDevice device);

typedef struct {
        DescriptorLayout *layout;
        VkDescriptorSet descriptor;
} Descriptor;

Descriptor descriptor_allocate(DescriptorLayout *layout);
void descriptor_free(Descriptor *descriptor);

void descriptor_write_image(Descriptor descriptor, Image *image, uint32_t binding,
                            uint32_t arr_index);
void descriptor_write_texture(Descriptor descriptor, Image *image, uint32_t binding,
                              uint32_t arr_index, VkSampler sampler);
void descriptor_write_buffer(Descriptor descriptor, buffer_t *buffer, uint32_t binding,
                             uint32_t arr_index);
