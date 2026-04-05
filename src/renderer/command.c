#include "renderer/command.h"

#include "renderer/vk_context.h"

typedef struct immediate_command {
        VkCommandPool pool;
        VkCommandBuffer command;
        VkFence fence;

        VkDevice device;
        VkQueue queue;
} immediate_command_t;

static immediate_command_t g_immediate_command;

void immediate_command_init() {
        g_immediate_command.queue = vk_context_graphics_queue();
        g_immediate_command.device = vk_context_device();
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0,
        };
        VK_EXPECT(
            vkCreateFence(vk_context_device(), &fence_info, NULL, &g_immediate_command.fence));

        VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = vk_context_queue_family_index(),
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        VK_EXPECT(vkCreateCommandPool(vk_context_device(), &command_pool_info, NULL,
                                      &g_immediate_command.pool));

        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = g_immediate_command.pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };
        VK_EXPECT(vkAllocateCommandBuffers(vk_context_device(), &alloc_info,
                                           &g_immediate_command.command));
}

void immediate_command_shutdown() {
        vkDestroyCommandPool(g_immediate_command.device, g_immediate_command.pool, NULL);
        vkDestroyFence(g_immediate_command.device, g_immediate_command.fence, NULL);
}

VkCommandBuffer immediate_command_begin() {
        VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(g_immediate_command.command, &begin);

        return g_immediate_command.command;
}

void immediate_command_end() {
        vkEndCommandBuffer(g_immediate_command.command);

        VkCommandBufferSubmitInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = g_immediate_command.command,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pCommandBufferInfos = &cmd_info,
            .commandBufferInfoCount = 1,
        };
        vkQueueSubmit2(g_immediate_command.queue, 1, &submit_info, g_immediate_command.fence);
        vkWaitForFences(g_immediate_command.device, 1, &g_immediate_command.fence, VK_TRUE,
                        1000000000);

        vkResetFences(g_immediate_command.device, 1, &g_immediate_command.fence);
        vkResetCommandPool(g_immediate_command.device, g_immediate_command.pool, 0);
}
