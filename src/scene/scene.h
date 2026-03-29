#pragma once

#include "camera.h"

#include "renderer/renderer.h"

typedef struct {
        vec3 position;
        vec3 rotation;
        vec3 scale;
} Transform;

typedef struct {
        Camera camera;

        Transform *transforms;
        GpuModel *models;

        bool mouse_left_down;
        float mouse_x, mouse_y;
} Scene;

Scene scene_create();
void scene_destroy(Scene *scene);

void scene_add_entity(Scene *scene, Transform transform, GpuModel model);
void scene_set_camera(Scene *scene, Camera camera);

void scene_draw(Scene *scene);
void scene_handle_inputs(Scene *scene);

void scene_handle_event(Scene *scene, SDL_Event e);
