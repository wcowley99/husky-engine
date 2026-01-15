#pragma once

#include "vkb.h"

#include <stdbool.h>

bool create_shader_module(VkDevice device, const uint32_t *bytes, size_t size,
                          VkShaderModule *module);

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
} GraphicsPipelineCreateInfo;

typedef struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} GraphicsPipeline;

bool graphics_pipeline_create(VkDevice device, GraphicsPipelineCreateInfo *create_info,
                              GraphicsPipeline *pipeline);

GraphicsPipeline create_pbr_pipeline(VkDevice device, VkDescriptorSetLayout global,
                                     VkDescriptorSetLayout material, VkFormat format);

void graphics_pipeline_destroy(GraphicsPipeline *pipeline, VkDevice device);

///////////////////////////////////////
/// Compute Pipeline
///////////////////////////////////////

typedef struct {
        VkDescriptorSetLayout *descriptors;
        uint32_t num_descriptors;

        const uint32_t *push_constant_sizes;
        uint32_t num_push_constant_sizes;

        const uint32_t *shader_source;
        uint32_t shader_source_size;
} ComputePipelineInfo;

typedef struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
} ComputePipeline;

bool compute_pipeline_create(VkDevice device, ComputePipelineInfo *info, ComputePipeline *p);

void compute_pipeline_destroy(ComputePipeline *p, VkDevice device);
