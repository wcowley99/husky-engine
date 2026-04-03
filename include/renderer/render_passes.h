#pragma once

#include "render_graph.h"

void gradient_pass_register(render_graph_t *graph, attachment_handle_t image);
void pbr_pass_register(render_graph_t *graph, attachment_handle_t hdr, attachment_handle_t depth);
void present_pass_register(render_graph_t *graph, attachment_handle_t image);
