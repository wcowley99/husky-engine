#include "command.h"

bool immediate_command_create(VkDevice device, uint32_t queue_family_index, VkQueue queue,
                              ImmediateCommand *command) {
        command->queue = queue;
        command->device = device;
        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = 0,
        };
        VK_EXPECT(vkCreateFence(device, &fence_info, NULL, &command->fence));

        VkCommandPoolCreateInfo command_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queue_family_index,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };
        VK_EXPECT(vkCreateCommandPool(device, &command_pool_info, NULL, &command->pool));

        VkCommandBufferAllocateInfo alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandBufferCount = 1,
            .commandPool = command->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        };
        VK_EXPECT(vkAllocateCommandBuffers(device, &alloc_info, &command->command));

        return true;
}

void immediate_command_destroy(ImmediateCommand *command) {
        vkDestroyCommandPool(command->device, command->pool, NULL);
        vkDestroyFence(command->device, command->fence, NULL);
}

void immediate_command_begin(ImmediateCommand *command) {
        VkCommandBufferBeginInfo begin = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(command->command, &begin);
}

void immediate_command_end(ImmediateCommand *command) {
        vkEndCommandBuffer(command->command);

        VkCommandBufferSubmitInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = command->command,
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pCommandBufferInfos = &cmd_info,
            .commandBufferInfoCount = 1,
        };
        vkQueueSubmit2(command->queue, 1, &submit_info, command->fence);
        vkWaitForFences(command->device, 1, &command->fence, VK_TRUE, 1000000000);

        vkResetFences(command->device, 1, &command->fence);
        vkResetCommandPool(command->device, command->pool, 0);
}
