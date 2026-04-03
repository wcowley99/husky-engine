#include "renderer/platform.h"

#include "renderer/vkb.h"

#include "husky.h"

#include <SDL3/SDL_vulkan.h>

typedef struct {
        SDL_Window *window;
        uint32_t width;
        uint32_t height;
} platform_t;

static platform_t g_platform;

void platform_init(uint32_t width, uint32_t height, const char *title) {
        VK_EXPECT(volkInitialize());

        if (!SDL_Init(SDL_INIT_VIDEO)) {
                ERROR("Failed to initialize SDL: %s\n", SDL_GetError());
                exit(1);
        }

        g_platform.window =
            SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

        if (!g_platform.window) {
                ERROR("Failed to create Window: %s", SDL_GetError());
                exit(1);
        }

        int w, h;
        SDL_GetWindowSize(g_platform.window, &w, &h);

        g_platform.width = w;
        g_platform.height = h;
}

void platform_shutdown(void) {
        SDL_DestroyWindow(g_platform.window);
        SDL_Quit();
}

VkSurfaceKHR platform_create_surface(VkInstance instance) {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(g_platform.window, instance, NULL, &surface)) {
                ERROR("SDL failed to create window: %s.", SDL_GetError());
                exit(1);
        }

        return surface;
}

void platform_get_size(uint32_t *width, uint32_t *height) {
        *width = g_platform.width;
        *height = g_platform.height;
}

bool platform_size_changed() {
        int w, h;
        SDL_GetWindowSize(g_platform.window, &w, &h);
        bool changed = w != g_platform.width || h != g_platform.height;

        g_platform.width = w;
        g_platform.height = h;

        return changed;
}

void platform_update_window() { SDL_UpdateWindowSurface(g_platform.window); }
