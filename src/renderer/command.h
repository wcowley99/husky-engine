#pragma once

#include "vkb.h"

#include <stdbool.h>

typedef struct {
        VkCommandPool pool;
        VkCommandBuffer command;
        VkFence fence;

        VkDevice device;
        VkQueue queue;
} ImmediateCommand;

bool immediate_command_create(VkDevice device, uint32_t queue_family_index, VkQueue queue,
                              ImmediateCommand *command);

void immediate_command_destroy(ImmediateCommand *command);

void immediate_command_begin(ImmediateCommand *command);
void immediate_command_end(ImmediateCommand *command);
