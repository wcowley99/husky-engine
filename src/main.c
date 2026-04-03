#include "renderer/renderer.h"
#include "world/world.h"

#include <SDL3/SDL.h>

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
        renderer_config_t config = {
            .width = 1920,
            .height = 1080,
            .title = "Civ Game",
            .debug = true,
        };
        if (!renderer_init(&config)) {
                return -1;
        }

        world_init();

        bool exit = false;
        while (!exit) {
                handle_events(&exit);

                renderer_begin_frame();

                world_progress();

                renderer_draw();

                renderer_end_frame();
        }

        world_shutdown();
        renderer_shutdown();
        return 0;
}
