#pragma once

#include "renderer/render_graph.h"

void gradient_pass_register(render_graph_t *graph, attachment_handle_t image);
void gradient_pass_cleanup();

void pbr_pass_register(render_graph_t *graph, attachment_handle_t hdr, attachment_handle_t depth);
void pbr_pass_cleanup();

void present_pass_register(render_graph_t *graph, attachment_handle_t image);
