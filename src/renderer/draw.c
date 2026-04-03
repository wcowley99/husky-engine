#include "renderer/draw.h"

#include "renderer/renderer.h"
#include "renderer/swapchain.h"

#include "common/array.h"

typedef struct {
        uint32_t mesh;
        material_t material;
        mat4 transform;

        bool recorded;
} RenderObject;

static RenderObject *g_render_objects;
static DrawBatch *g_draw_batches;

void renderable_record(GpuModel model, mat4 transform) {
        for (int i = 0; i < array_length(model.meshes); i++) {
                GpuMesh mesh = model.meshes[i];
                RenderObject obj = {
                    .material = mesh.material,
                    .mesh = mesh.mesh,
                    .recorded = false,
                };
                glm_mat4_dup(transform, obj.transform);

                array_append(g_render_objects, obj);
        }
}

bool render_object_compatible(RenderObject a, RenderObject b) { return (a.mesh == b.mesh); }

void draw_buffers_init() {
        g_render_objects = array(RenderObject);
        g_draw_batches = array(DrawBatch);
}

void draw_buffers_shutdown() {
        array_free(g_draw_batches);
        array_free(g_render_objects);
}

void draw_buffers_clear() {
        array_header(g_draw_batches)->length = 0;
        array_header(g_render_objects)->length = 0;
}

static void render_object_compact(RenderObject obj, Instance *ssbo, uint32_t *instance_count) {
        DrawBatch draw_batch = {.mesh = obj.mesh, .first_instance = *instance_count};

        for (int i = 0; i < array_length(g_render_objects); i++) {
                if (g_render_objects[i].mesh == obj.mesh) {
                        ssbo[*instance_count] = (Instance){
                            .vertex_address = gpu_mesh_vertex_address(obj.mesh),
                            .tex_index = obj.material.diffuse_tex,
                        };
                        glm_mat4_dup(obj.transform, ssbo[*instance_count].model);

                        draw_batch.count++;
                        (*instance_count)++;
                        g_render_objects[i].recorded = true;
                }
        }
        array_append(g_draw_batches, draw_batch);
}

void draw_batches_upload() {
        Instance *ssbo = (Instance *)swapchain_current_frame_get_buffer(FRAME_BUFFER_INSTANCES);

        uint32_t instance_count = 0;
        for (int i = 0; i < array_length(g_render_objects); i++) {
                if (!g_render_objects[i].recorded)
                        render_object_compact(g_render_objects[i], ssbo, &instance_count);
        }

        swapchain_current_frame_unmap_buffer(FRAME_BUFFER_INSTANCES);
}

void draw_batches_record() {
        for (int i = 0; i < array_length(g_draw_batches); i++) {
                uint32_t mesh = g_draw_batches[i].mesh;
                vkCmdBindIndexBuffer(swapchain_current_frame_command_buffer(),
                                     gpu_mesh_index_buffer(mesh), 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(swapchain_current_frame_command_buffer(),
                                 gpu_mesh_index_count(mesh), g_draw_batches[i].count, 0, 0,
                                 g_draw_batches[i].first_instance);
        }
}
