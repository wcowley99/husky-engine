#pragma once

#include "husky.h"

#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <volk.h>

#include <cglm/cglm.h>

#define MAX_INSTANCES 100000
#define MAX_TEXTURES 5000000

#define NUM_FRAMES 3

#define VK_EXPECT(x)                                                                               \
        do {                                                                                       \
                VkResult err = x;                                                                  \
                if (err != VK_SUCCESS) {                                                           \
                        ERROR("VK_EXPECT failed: %s", string_VkResult(err));                       \
                        exit(1);                                                                   \
                }                                                                                  \
        } while (0)

typedef struct {
        mat4 view;
        mat4 proj;
        mat4 viewproj;

        vec4 ambientColor;
        vec4 sunlightDirection;
        vec4 sunlightColor;
} SceneData;

typedef struct {
        mat4 model;
        VkDeviceAddress vertex_address;
        int tex_index;
        int padding;
} Instance;
