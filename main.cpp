#include "render/engine.h"
#include "render/window.h"

#include <SDL3/SDL.h>

#include <iostream>

int main(int argc, char *argv[]) {
  Render::Window window("Husky Engine", 1920, 1080);

  Render::Engine engine(window);

  SDL_Event e;
  bool quit = false;
  while (true) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT ||
          e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
        quit = true;
        break;
      }
    }

    if (quit) {
      break;
    }

    engine.draw();
    window.refresh();
  }

  return 0;
}
