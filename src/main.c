#include "renderer.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

bool should_quit(SDL_Event *e) {
  return e->type == SDL_EVENT_QUIT ||
         (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_ESCAPE);
}

int main(int argc, char **argv) {
  RendererCreateInfo create_info = {
      .width = 1920, .height = 1080, .title = "Mordor", .debug = true};
  Renderer r = {};
  if (!renderer_init(&r, &create_info)) {
    return -1;
  }

  bool exit = false;
  SDL_Event e;
  while (!exit) {
    while (SDL_PollEvent(&e)) {
      if (should_quit(&e)) {
        exit = true;
      }
    }

    renderer_draw(&r);
  }

  renderer_shutdown(&r);
  return 0;
}
