#include "scene.h"

#include "common/array.h"
#include "renderer/renderer.h"

#include <SDL3/SDL.h>

#include <cglm/affine.h>

void transform_to_model_matrix(Transform transform, mat4 model) {
        glm_mat4_identity(model);
        glm_translate(model, transform.position);
        glm_scale(model, transform.scale);
}

Scene scene_create() {
        Scene r = {0};
        r.transforms = array(Transform);
        r.models = array(GpuModel);
        return r;
}
void scene_destroy(Scene *scene) {
        array_free(scene->transforms);
        array_free(scene->models);
}

void scene_add_entity(Scene *scene, Transform transform, GpuModel model) {
        array_append(scene->transforms, transform);
        array_append(scene->models, model);
}

void scene_set_camera(Scene *scene, Camera camera) { scene->camera = camera; }

void scene_draw(Scene *scene) {
        agpu_set_camera(scene->camera);

        for (int i = 0; i < array_length(scene->transforms); i += 1) {
                mat4 model;
                transform_to_model_matrix(scene->transforms[i], model);

                agpu_draw_model(scene->models[i], model);
        }
}

void scene_handle_inputs(Scene *scene) {
        const bool *keys = SDL_GetKeyboardState(NULL);
        const float camera_speed = 0.02f;

        Camera *camera = &scene->camera;

        if (keys[SDL_SCANCODE_W]) {
                vec3 delta;
                glm_vec3_scale(camera->target, camera_speed, delta);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_A]) {
                vec3 delta;
                glm_vec3_cross(camera->target, camera->up, delta);
                glm_vec3_normalize(delta);
                glm_vec3_scale(delta, -camera_speed, delta);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_S]) {
                vec3 delta;
                glm_vec3_scale(camera->target, -camera_speed, delta);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_D]) {
                vec3 delta;
                glm_vec3_cross(camera->target, camera->up, delta);
                glm_vec3_normalize(delta);
                glm_vec3_scale(delta, camera_speed, delta);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_SPACE]) {
                vec3 delta;
                glm_vec3_scale(camera->up, camera_speed, delta);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_LSHIFT]) {
                vec3 delta;
                glm_vec3_scale(camera->up, -camera_speed, delta);
                camera_move(camera, delta);
        }
        if (scene->mouse_left_down) {
                // vec3 rot = vec3_scale((vec3){scene->mouse_x, scene->mouse_y, 0}, 0.1f);
                // camera_rotate(rot);
                // printf("{%f, %f}\n", scene->mouse_x, scene->mouse_y);
        }
}

void scene_handle_event(Scene *scene, SDL_Event e) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                scene->mouse_left_down = true;
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
                scene->mouse_left_down = false;
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                scene->mouse_x = e.motion.xrel;
                scene->mouse_y = e.motion.yrel;
        }
}
