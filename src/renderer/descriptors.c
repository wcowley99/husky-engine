#include "descriptors.h"

#include "vk_context.h"
#include "vkb.h"

#include "common/array.h"

#include <stdlib.h>

typedef struct {
        VkDescriptorPool pool;

        VkDescriptorPoolSize *pool_sizes;
        uint32_t num_frames;

        VkDevice device;
} DescriptorAllocator;

static DescriptorAllocator g_descriptor_allocator;

static DescriptorLayout g_global_descriptor_layout;

DescriptorLayout descriptor_layout_create(VkDevice device, DescriptorBinding *bindings,
                                          uint32_t count) {
        DescriptorLayout r = {0};
        r.binding_types = array(VkDescriptorType);
        r.binding_counts = array(uint32_t);
        r.variable = array(bool);
        r.device = device;

        VkDescriptorSetLayoutBinding *vk_bindings =
            calloc(count, sizeof(VkDescriptorSetLayoutBinding));
        VkDescriptorBindingFlags *flags = calloc(count, sizeof(VkDescriptorBindingFlags));

        bool any_variable = false;
        uint32_t total_descriptors = 0;

        for (uint32_t i = 0; i < count; i += 1) {
                array_append(r.binding_types, bindings[i].type);
                array_append(r.binding_counts, bindings[i].count);
                array_append(r.variable, bindings[i].variable);

                vk_bindings[i].binding = i;
                vk_bindings[i].descriptorType = bindings[i].type;
                vk_bindings[i].descriptorCount = bindings[i].count;
                vk_bindings[i].stageFlags = bindings[i].stage;

                if (bindings[i].variable) {
                        any_variable = true;
                        flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                   VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                   VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                } else {
                        flags[i] = 0;
                }
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = count,
            .pBindingFlags = flags,
        };

        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pBindings = vk_bindings,
            .bindingCount = count,
            .flags = any_variable ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0,
            .pNext = &binding_info,
        };

        vkCreateDescriptorSetLayout(device, &create_info, NULL, &r.layout);

        free(vk_bindings);
        free(flags);

        return r;
}

void descriptor_layout_destroy(DescriptorLayout *layout) {
        vkDestroyDescriptorSetLayout(layout->device, layout->layout, NULL);
        array_free(layout->binding_types);
        array_free(layout->binding_counts);
        array_free(layout->variable);
}

void global_descriptor_layout_init() {
        DescriptorBinding global_bindings[3] = {
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .count = 1,
             .stage = VK_SHADER_STAGE_ALL_GRAPHICS},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             .count = 1,
             .stage = VK_SHADER_STAGE_VERTEX_BIT},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .count = MAX_TEXTURES,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .variable = true},
        };
        uint32_t count = sizeof(global_bindings) / sizeof(DescriptorBinding);

        descriptor_allocator_reserve(global_bindings, count, true);
        g_global_descriptor_layout =
            descriptor_layout_create(vk_context_device(), global_bindings, 3);
}
DescriptorLayout *global_descriptor_layout() { return &g_global_descriptor_layout; }

void descriptor_allocator_init(uint32_t num_frames) {
        g_descriptor_allocator.pool_sizes = array(VkDescriptorPoolSize);
        g_descriptor_allocator.num_frames = num_frames;
}

void descriptor_allocator_destroy() {
        vkDestroyDescriptorPool(g_descriptor_allocator.device, g_descriptor_allocator.pool, NULL);
        array_free(g_descriptor_allocator.pool_sizes);
}

void descriptor_allocator_clear() {
        vkResetDescriptorPool(g_descriptor_allocator.device, g_descriptor_allocator.pool, 0);
}

void descriptor_allocator_reserve(DescriptorBinding *bindings, uint32_t count,
                                  bool per_frame_descriptor) {
        for (int b = 0; b < count; b += 1) {
                VkDescriptorType type = bindings[b].type;
                uint32_t count = bindings[b].count *
                                 (per_frame_descriptor ? g_descriptor_allocator.num_frames : 1);
                bool found = false;
                for (int i = 0; i < array_length(g_descriptor_allocator.pool_sizes); i += 1) {
                        if (g_descriptor_allocator.pool_sizes[i].type == type) {
                                g_descriptor_allocator.pool_sizes[i].descriptorCount += count;
                                found = true;
                        }
                }

                if (!found) {
                        VkDescriptorPoolSize size = {
                            .type = type,
                            .descriptorCount = count,
                        };
                        array_append(g_descriptor_allocator.pool_sizes, size);
                }
        }
}

bool descriptor_allocator_create(VkDevice device) {
        g_descriptor_allocator.device = device;

        uint32_t sum = 0;
        for (uint32_t i = 0; i < array_length(g_descriptor_allocator.pool_sizes); i += 1) {
                sum += g_descriptor_allocator.pool_sizes[i].descriptorCount;
        }

        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pPoolSizes = g_descriptor_allocator.pool_sizes,
            .poolSizeCount = array_length(g_descriptor_allocator.pool_sizes),
            .maxSets = sum,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                     VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        };

        VK_EXPECT(vkCreateDescriptorPool(device, &create_info, NULL, &g_descriptor_allocator.pool));

        return true;
}

Descriptor descriptor_allocate(DescriptorLayout *layout) {
        Descriptor r = {0};
        r.layout = layout;

        uint32_t var_count = 0;
        for (int i = 0; i < array_length(layout->variable); i += 1) {
                if (layout->variable[i]) {
                        var_count += layout->binding_counts[i];
                }
        }

        VkDescriptorSetVariableDescriptorCountAllocateInfo var_bindings = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &var_count,
        };

        VkDescriptorSetAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = g_descriptor_allocator.pool,
            .pSetLayouts = &layout->layout,
            .descriptorSetCount = 1,
            .pNext = var_count ? &var_bindings : VK_NULL_HANDLE,
        };

        vkAllocateDescriptorSets(g_descriptor_allocator.device, &alloc_info, &r.descriptor);

        return r;
}

void descriptor_free(Descriptor *descriptor) {
        vkFreeDescriptorSets(vk_context_device(), g_descriptor_allocator.pool, 1,
                             &descriptor->descriptor);
}

void descriptor_write_image(Descriptor descriptor, Image *image, uint32_t binding,
                            uint32_t arr_index) {
        VkDescriptorImageInfo image_info = {
            .sampler = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .imageView = image->image_view,
        };

        VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstSet = descriptor.descriptor,
            .dstArrayElement = arr_index,
            .descriptorCount = 1,
            .descriptorType = descriptor.layout->binding_types[binding],
            .pImageInfo = &image_info,
        };

        vkUpdateDescriptorSets(descriptor.layout->device, 1, &write_set, 0, NULL);
}

void descriptor_write_texture(Descriptor descriptor, Image *image, uint32_t binding,
                              uint32_t arr_index, VkSampler sampler) {
        VkDescriptorImageInfo image_info = {
            .sampler = sampler,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = image->image_view,
        };

        VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstSet = descriptor.descriptor,
            .dstArrayElement = arr_index,
            .descriptorCount = 1,
            .descriptorType = descriptor.layout->binding_types[binding],
            .pImageInfo = &image_info,
        };

        vkUpdateDescriptorSets(descriptor.layout->device, 1, &write_set, 0, NULL);
}

void descriptor_write_buffer(Descriptor descriptor, Buffer *buffer, uint32_t binding,
                             uint32_t arr_index) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = buffer->buffer,
            .offset = 0,
            .range = buffer->info.size,
        };

        VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstSet = descriptor.descriptor,
            .dstArrayElement = arr_index,
            .descriptorCount = 1,
            .descriptorType = descriptor.layout->binding_types[binding],
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(descriptor.layout->device, 1, &write_set, 0, NULL);
}
