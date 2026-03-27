#include "renderer/renderer.h"

#include "scene/camera.h"
#include "scene/scene.h"

#include <SDL3/SDL.h>

#include <stdio.h>

Scene g_Scene;

bool should_quit(SDL_Event *e) {
        return e->type == SDL_EVENT_QUIT ||
               (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_ESCAPE);
}

void handle_events(bool *exit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
                if (should_quit(&e)) {
                        *exit = true;
                } else {
                        scene_handle_event(&g_Scene, e);
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

        // ModelHandle box = agpu_load_model("assets/BoxTextured/glTF-Binary/BoxTextured.glb");
        // ModelHandle suzanne = agpu_load_model("assets/Suzanne/glTF/Suzanne.gltf");
        ModelHandle bard = agpu_load_model("assets/bard-1.obj");
        // ModelHandle sponze = agpu_load_model("assets/Sponza/glTF/Sponza.gltf");

        g_Scene = scene_create();
        scene_set_camera(&g_Scene, camera_default());

        Transform transform = {0};
        transform.scale[0] = 1.0f;
        transform.scale[1] = 1.0f;
        transform.scale[2] = 1.0f;
        // scene_add_entity(&g_Scene, transform, sponze);

        for (int i = 0; i < 10; i += 1) {
                transform.position[0] = 1.0f;
                transform.position[1] = 0.0f;
                transform.position[2] = -i;

                transform.scale[0] = i / 3.0f;
                transform.scale[1] = i / 3.0f;
                transform.scale[2] = i / 3.0f;
                scene_add_entity(&g_Scene, transform, bard);

                transform.position[0] = -1.0f;

                transform.scale[0] = (10 - i) / 10.0f;
                transform.scale[1] = (10 - i) / 10.0f;
                transform.scale[2] = (10 - i) / 10.0f;
                // scene_add_entity(&g_Scene, transform, suzanne);
        }

        bool exit = false;
        while (!exit) {
                handle_events(&exit);

                scene_handle_inputs(&g_Scene);

                // draw
                agpu_begin_frame();

                scene_draw(&g_Scene);

                agpu_end_frame();
        }

        RendererShutdown();
        scene_destroy(&g_Scene);
        return 0;
}
