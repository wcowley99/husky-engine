#include "sampler.h"

#include "vk_context.h"

static VkSampler g_linear_sampler;
static VkSampler g_nearest_sampler;

void samplers_init() {
        VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;

        vkCreateSampler(vk_context_device(), &sampler_info, NULL, &g_linear_sampler);

        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;

        vkCreateSampler(vk_context_device(), &sampler_info, NULL, &g_nearest_sampler);
}
void samplers_shutdown() {
        vkDestroySampler(vk_context_device(), g_linear_sampler, NULL);
        vkDestroySampler(vk_context_device(), g_nearest_sampler, NULL);
}

VkSampler linear_sampler() { return g_linear_sampler; }
