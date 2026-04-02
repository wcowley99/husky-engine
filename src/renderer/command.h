#pragma once

#include "vkb.h"

#include <stdbool.h>

void immediate_command_init();
void immediate_command_shutdown();

VkCommandBuffer immediate_command_begin();
void immediate_command_end();
