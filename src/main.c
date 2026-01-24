#include "renderer/renderer.h"

#include "scene/camera.h"

#include <SDL3/SDL.h>

#include <stdio.h>

bool should_quit(SDL_Event *e) {
        return e->type == SDL_EVENT_QUIT ||
               (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_ESCAPE);
}

bool handle_move(Camera *camera) {
        const bool *keys = SDL_GetKeyboardState(NULL);

        const float camera_speed = 0.02f;

        if (keys[SDL_SCANCODE_W]) {
                vec3 delta = vec3_scale(camera->target, camera_speed);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_A]) {
                vec3 delta = vec3_scale(vec3_normalize(vec3_cross(camera->target, camera->up)),
                                        -camera_speed);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_S]) {
                vec3 delta = vec3_scale(camera->target, -camera_speed);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_D]) {
                vec3 delta = vec3_scale(vec3_normalize(vec3_cross(camera->target, camera->up)),
                                        camera_speed);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_SPACE]) {
                vec3 delta = vec3_scale(camera->up, camera_speed);
                camera_move(camera, delta);
        }
        if (keys[SDL_SCANCODE_LSHIFT]) {
                vec3 delta = vec3_scale(camera->up, -camera_speed);
                camera_move(camera, delta);
        }

        return false;
}

void handle_events(bool *exit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
                if (should_quit(&e)) {
                        *exit = true;
                }
        }
}

int main(int argc, char **argv) {
        RendererCreateInfo create_info = {
            .width = 1920,
            .height = 1080,
            .title = "Civ Game",
            .debug = true,
        };
        if (!RendererInit(&create_info)) {
                return -1;
        }

        Camera camera = {
            .position = (vec3){0.0f, 0.0f, 2.0f},
            .target = (vec3){0.0f, 0.0f, -1.0f},
            .up = (vec3){0.0f, 1.0f, 0.0f},
            .fov = 45.0f,
        };

        ModelHandle red_guy = agpu_load_model("assets/objs/red-demon.obj");
        ModelHandle stone_guy = agpu_load_model("assets/objs/stone-golem.obj");

        vec3 red_posns[10];
        vec3 stone_posns[10];

        for (int i = 0; i < 10; i += 1) {
                red_posns[i] = (vec3){1, 0, -i};
                stone_posns[i] = (vec3){-1, 0, -i};
        }

        bool exit = false;
        while (!exit) {
                handle_events(&exit);

                handle_move(&camera);

                // draw
                agpu_begin_frame();
                agpu_set_camera(camera);

                for (int i = 0; i < 10; i += 1) {
                        agpu_draw_model(red_guy, mat4_translate(MAT4_IDENTITY, red_posns[i]));
                        agpu_draw_model(stone_guy, mat4_translate(MAT4_IDENTITY, stone_posns[i]));
                }

                agpu_end_frame();
        }

        RendererShutdown();
        return 0;
}
