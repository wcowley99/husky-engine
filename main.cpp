#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include <SDL3/SDL.h>
#include <iostream>

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL could not initialize! SDL error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("Hello, World", 1920, 1080, 0);
  if (!window) {
    SDL_Log("SDL could not create window! SDL error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Surface *surface = SDL_GetWindowSurface(window);

  SDL_Event e;
  bool quit = false;
  while (!quit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        quit = true;
        break;
      }
    }

    SDL_FillSurfaceRect(surface, nullptr,
                        SDL_MapSurfaceRGB(surface, 0xFF, 0x00, 0x00));

    SDL_UpdateWindowSurface(window);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
