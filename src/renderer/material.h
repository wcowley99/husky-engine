#pragma once

#include "pipeline.h"

#include "vkb.h"

typedef enum {
        MATERIAL_PASS_COLOR,
        MATERIAL_PASS_TRANSPARENT,
        MATERIAL_PASS_OTHER,
} MaterialPass;

typedef struct {
        GraphicsPipeline *pipeline;
        VkDescriptorSet material_set;
        MaterialPass pass;
} Material;

Material build_default_material();
