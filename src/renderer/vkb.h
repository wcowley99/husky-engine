#pragma once

#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <volk.h>

#include <stdio.h>

#define EXPECT(x)                                                                                  \
        do {                                                                                       \
                if (!(x)) {                                                                        \
                        printf("[%s:%d] EXPECT failed: %s\n", __FILE__, __LINE__, #x);             \
                        return false;                                                              \
                }                                                                                  \
        } while (false)

#define VK_EXPECT(x)                                                                               \
        do {                                                                                       \
                VkResult err = x;                                                                  \
                if (err != VK_SUCCESS) {                                                           \
                        printf("[%s:%d] VK_EXPECT failed: %s\n", __FILE__, __LINE__,               \
                               string_VkResult(err));                                              \
                        return false;                                                              \
                }                                                                                  \
        } while (false)
