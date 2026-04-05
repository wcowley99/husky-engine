#pragma once

#include "vkb.h"

#include <stdbool.h>

///////////////////////////////////////
/// Graphics Pipeline
///////////////////////////////////////

typedef struct {
        VkDescriptorSetLayout *descriptors;
        uint32_t num_descriptors;

        const VkPushConstantRange *push_constants;
        uint32_t num_push_constants;

        VkPrimitiveTopology topology;
        VkPolygonMode polygon_mode;
        VkCullModeFlagBits cull_mode;
        VkFrontFace front_face;
        VkFormat color_attachment_format;
        VkFormat depth_attachment_format;

        const uint32_t *vertex_shader;
        uint32_t vertex_shader_size;

        const uint32_t *fragment_shader;
        uint32_t fragment_shader_size;

        bool depth_testing;
        VkCompareOp depth_compare_op;
} graphics_pipeline_config_t;

typedef struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} graphics_pipeline_t;

graphics_pipeline_t graphics_pipeline_create(VkDevice device,
                                             graphics_pipeline_config_t *create_info);
void graphics_pipeline_destroy(graphics_pipeline_t *pipeline, VkDevice device);

///////////////////////////////////////
/// Compute Pipeline
///////////////////////////////////////

typedef struct compute_pipeline_config {
        VkDescriptorSetLayout *descriptors;
        uint32_t num_descriptors;

        const uint32_t *push_constant_sizes;
        uint32_t num_push_constant_sizes;

        const uint32_t *shader_source;
        uint32_t shader_source_size;
} comnpute_pipeline_config_t;

typedef struct compute_pipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} compute_pipeline_t;

compute_pipeline_t compute_pipeline_create(VkDevice device, comnpute_pipeline_config_t *info);
void compute_pipeline_destroy(compute_pipeline_t *p, VkDevice device);
