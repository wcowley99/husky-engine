#include "renderer.h"

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
            .title = "Mordor",
            .debug = true,
        };
        if (!RendererInit(&create_info)) {
                return -1;
        }

        bool exit = false;
        while (!exit) {
                handle_events(&exit);

                handle_move();

                RendererDraw();
        }

        RendererShutdown();
        return 0;
}
