#include "renderer.h"

#include <SDL3/SDL.h>

#include <stdio.h>

bool should_quit(SDL_Event *e) {
        return e->type == SDL_EVENT_QUIT ||
               (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_ESCAPE);
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

                RendererDraw();
        }

        RendererShutdown();
        return 0;
}
