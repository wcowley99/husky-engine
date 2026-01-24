#include "renderer/renderer.h"

#include <SDL3/SDL.h>

#include <stdio.h>

bool should_quit(SDL_Event *e) {
        return e->type == SDL_EVENT_QUIT ||
               (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_ESCAPE);
}

bool handle_move() {
        const bool *keys = SDL_GetKeyboardState(NULL);

        if (keys[SDL_SCANCODE_W]) {
                vec3 delta = {.x = 0.0f, .y = 0.0f, .z = 0.02f};
                MoveCamera(delta);
        }
        if (keys[SDL_SCANCODE_A]) {
                vec3 delta = {.x = -0.02f, .y = 0.0f, .z = 0.0f};
                MoveCamera(delta);
        }
        if (keys[SDL_SCANCODE_S]) {
                vec3 delta = {.x = 0.0f, .y = 0.0f, .z = -0.02f};
                MoveCamera(delta);
        }
        if (keys[SDL_SCANCODE_D]) {
                vec3 delta = {.x = 0.02f, .y = 0.0f, .z = 0.00f};
                MoveCamera(delta);
        }
        if (keys[SDL_SCANCODE_SPACE]) {
                vec3 delta = {.x = 0.0f, .y = 0.02f, .z = 0.0f};
                MoveCamera(delta);
        }
        if (keys[SDL_SCANCODE_LSHIFT]) {
                vec3 delta = {.x = 0.0f, .y = -0.02f, .z = 0.0f};
                MoveCamera(delta);
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

                handle_move();

                // draw
                agpu_begin_frame();

                for (int i = 0; i < 10; i += 1) {
                        agpu_draw_model(red_guy, mat4_translate(MAT4_IDENTITY, red_posns[i]));
                        agpu_draw_model(stone_guy, mat4_translate(MAT4_IDENTITY, stone_posns[i]));
                }

                agpu_end_frame();
        }

        RendererShutdown();
        return 0;
}
