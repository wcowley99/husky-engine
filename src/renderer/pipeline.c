#include "renderer/pipeline.h"

#include <stdlib.h>

static void create_shader_module(VkDevice device, const uint32_t *bytes, size_t len,
                                 VkShaderModule *module) {
        VkShaderModuleCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pCode = bytes,
            .codeSize = len * sizeof(uint32_t),
        };

        VK_EXPECT(vkCreateShaderModule(device, &create_info, NULL, module));
}

graphics_pipeline_t graphics_pipeline_create(VkDevice device,
                                             graphics_pipeline_config_t *create_info) {
        graphics_pipeline_t p = {0};

        VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineColorBlendAttachmentState color_attachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        VkPipelineColorBlendStateCreateInfo color_blend = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
        };

        VkPipelineDepthStencilStateCreateInfo depth_testing = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = create_info->depth_testing,
            .depthWriteEnable = create_info->depth_testing,
            .depthCompareOp = create_info->depth_compare_op,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };

        VkPipelineVertexInputStateCreateInfo vertex_input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .primitiveRestartEnable = VK_FALSE,
            .topology = create_info->topology,
        };

        VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        VkPipelineRenderingCreateInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &create_info->color_attachment_format,
            .depthAttachmentFormat = create_info->depth_attachment_format,
        };

        VkPipelineRasterizationStateCreateInfo rasterization = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = create_info->polygon_mode,
            .lineWidth = 1.0f,
            .cullMode = create_info->cull_mode,
            .frontFace = create_info->front_face,
        };

        VkDynamicState state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = state,
        };

        VkShaderModule vertex;
        VkShaderModule fragment;
        create_shader_module(device, create_info->vertex_shader, create_info->vertex_shader_size,
                             &vertex);
        create_shader_module(device, create_info->fragment_shader,
                             create_info->fragment_shader_size, &fragment);

        VkPipelineShaderStageCreateInfo shaders[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertex,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment,
                .pName = "main",
            },
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = create_info->descriptors,
            .setLayoutCount = create_info->num_descriptors,
            .pPushConstantRanges = create_info->push_constants,
            .pushConstantRangeCount = create_info->num_push_constants,
        };

        DEBUG("graphics descriptor: %p", *create_info->descriptors);
        VK_EXPECT(vkCreatePipelineLayout(device, &layout_info, NULL, &p.layout));

        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaders,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisample,
            .pColorBlendState = &color_blend,
            .pDepthStencilState = &depth_testing,
            .layout = p.layout,
            .pDynamicState = &dynamic_state,
            .pNext = &render_info,
        };

        VK_EXPECT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
                                            &p.pipeline));

        vkDestroyShaderModule(device, vertex, NULL);
        vkDestroyShaderModule(device, fragment, NULL);

        return p;
}

void graphics_pipeline_destroy(graphics_pipeline_t *pipeline, VkDevice device) {
        vkDestroyPipelineLayout(device, pipeline->layout, NULL);
        vkDestroyPipeline(device, pipeline->pipeline, NULL);
}

///////////////////////////////////////
/// Compute Pipeline
///////////////////////////////////////

compute_pipeline_t compute_pipeline_create(VkDevice device, comnpute_pipeline_config_t *info) {
        compute_pipeline_t p = {0};

        VkPushConstantRange *ranges =
            malloc(info->num_push_constant_sizes * sizeof(VkPushConstantRange));

        for (uint32_t i = 0; i < info->num_push_constant_sizes; i += 1) {
                ranges[i].size = info->push_constant_sizes[i];
                ranges[i].offset = 0;
                ranges[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        DEBUG("compute descriptor: %p", *info->descriptors);
        VkPipelineLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pSetLayouts = info->descriptors,
            .setLayoutCount = info->num_descriptors,
            .pPushConstantRanges = ranges,
            .pushConstantRangeCount = info->num_push_constant_sizes,
        };

        VkResult result = vkCreatePipelineLayout(device, &layout_create_info, NULL, &p.layout);
        free(ranges);
        VK_EXPECT(result);

        VkShaderModule compute;
        create_shader_module(device, info->shader_source, info->shader_source_size, &compute);

        VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = compute,
                    .pName = "main",
                },
            .layout = p.layout,
        };

        result =
            vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &p.pipeline);
        vkDestroyShaderModule(device, compute, NULL);
        VK_EXPECT(result);

        return p;
}

void compute_pipeline_destroy(compute_pipeline_t *p, VkDevice device) {
        vkDestroyPipelineLayout(device, p->layout, NULL);
        vkDestroyPipeline(device, p->pipeline, NULL);
}
