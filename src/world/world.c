#include "world/world.h"

#include "renderer/camera.h"
#include "renderer/renderer.h"

#include <SDL3/SDL.h>
#include <cglm/cglm.h>
#include <flecs.h>

static ecs_world_t *ecs;

typedef struct position {
        vec3 position;
} position_t;

typedef struct scale {
        vec3 scale;
} scale_t;

typedef struct camera_target {
        vec3 target;
} camera_target_t;

typedef struct model_component {
        GpuModel model;
} model_component_t;

static void renderables_collect(ecs_iter_t *it) {
        position_t *p = ecs_field(it, position_t, 0);
        scale_t *s = ecs_field(it, scale_t, 1);
        model_component_t *m = ecs_field(it, model_component_t, 2);

        for (int i = 0; i < it->count; i++) {
                mat4 model;
                glm_mat4_identity(model);
                glm_translate(model, p[i].position);
                glm_scale(model, s[i].scale);

                renderable_record(m[i].model, model);
        }
}

static void set_camera(ecs_iter_t *it) {
        position_t *p = ecs_field(it, position_t, 0);
        camera_target_t *t = ecs_field(it, camera_target_t, 1);

        camera_t camera = {
            .fov = 45.0f,
            .up = {0.0f, 1.0f, 0.0f},
        };
        glm_vec3_copy(p->position, camera.position);
        glm_vec3_copy(t->target, camera.target);

        renderer_set_camera(camera);
}

static void move(ecs_iter_t *it) {
        position_t *p = ecs_field(it, position_t, 0);
        camera_target_t *t = ecs_field(it, camera_target_t, 1);

        for (int i = 0; i < it->count; i++) {
                const bool *keys = SDL_GetKeyboardState(NULL);
                const float camera_speed = 0.02f;

                if (keys[SDL_SCANCODE_W]) {
                        vec3 delta;
                        glm_vec3_scale(t[i].target, camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
                if (keys[SDL_SCANCODE_A]) {
                        vec3 delta;
                        glm_vec3_cross(t[i].target, (vec3){0.0f, 1.0f, 0.0f}, delta);
                        glm_vec3_normalize(delta);
                        glm_vec3_scale(delta, -camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
                if (keys[SDL_SCANCODE_S]) {
                        vec3 delta;
                        glm_vec3_scale(t[i].target, -camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
                if (keys[SDL_SCANCODE_D]) {
                        vec3 delta;
                        glm_vec3_cross(t[i].target, (vec3){0.0f, 1.0f, 0.0f}, delta);
                        glm_vec3_normalize(delta);
                        glm_vec3_scale(delta, camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
                if (keys[SDL_SCANCODE_SPACE]) {
                        vec3 delta;
                        glm_vec3_scale((vec3){0.0f, 1.0f, 0.0f}, camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
                if (keys[SDL_SCANCODE_LSHIFT]) {
                        vec3 delta;
                        glm_vec3_scale((vec3){0.0f, 1.0f, 0.0f}, -camera_speed, delta);
                        glm_vec3_add(p[i].position, delta, p[i].position);
                }
        }
}

void world_init() {
        GpuModel model = renderer_load_model("assets/Sponza/glTF/Sponza.gltf");

        ecs = ecs_init();
        ECS_COMPONENT(ecs, position_t);
        ECS_COMPONENT(ecs, scale_t);
        ECS_COMPONENT(ecs, camera_target_t);
        ECS_COMPONENT(ecs, model_component_t);

        ECS_SYSTEM(ecs, move, EcsOnUpdate, position_t, [in] camera_target_t);

        ECS_SYSTEM(ecs, set_camera, EcsPostUpdate, [in] position_t, [in] camera_target_t);
        ECS_SYSTEM(ecs, renderables_collect,
                   EcsPostUpdate, [in] position_t, [in] scale_t, [in] model_component_t);

        ecs_entity_t player = ecs_new(ecs);
        ecs_set(ecs, player, position_t, {0.0f, 0.0f, 0.0f});
        ecs_set(ecs, player, camera_target_t, {1.0f, 0.0f, 0.0f});

        ecs_entity_t sponza = ecs_new(ecs);
        ecs_set(ecs, sponza, position_t, {0.0f, 0.0f, 0.0f});
        ecs_set(ecs, sponza, scale_t, {0.01f, 0.01f, 0.01f});
        ecs_set(ecs, sponza, model_component_t, {model});
}

void world_progress() { ecs_progress(ecs, 0.016f); }

void world_shutdown() { ecs_fini(ecs); }
