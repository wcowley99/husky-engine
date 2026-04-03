#include "renderer/camera.h"

#include "renderer/platform.h"
#include "renderer/swapchain.h"

void renderer_set_camera(Camera camera) {
        SceneData *scene = swapchain_current_frame_get_buffer(FRAME_BUFFER_CAMERA);

        // Vulkan internally uses an inverted Y-axis compared to cglm. That is, y=0 is the top of
        // the screen, while y=MAX is the bottom. To adjust for this we need to invert the view
        // matrix along the y axis. Now, we also need to flip the position of the camera eye on the
        // y-axis or vertical movement will be inverted

        vec3 eye = {camera.position[0], -1 * camera.position[1], camera.position[2]};
        vec3 flip = {1.0f, -1.0f, 1.0f};

        glm_look(eye, camera.target, camera.up, scene->view);
        glm_scale(scene->view, flip);

        uint32_t w, h;
        platform_get_size(&w, &h);

        glm_perspective(glm_rad(camera.fov), (float)w / h, 0.1f, 100.0f, scene->proj);

        glm_mat4_mul(scene->proj, scene->view, scene->viewproj);

        scene->ambientColor[0] = 1.0f;
        scene->ambientColor[1] = 1.0f;
        scene->ambientColor[2] = 1.0f;
        scene->ambientColor[3] = 0.0f;

        scene->sunlightDirection[0] = 0.3f;
        scene->sunlightDirection[1] = 1.0f;
        scene->sunlightDirection[2] = 0.3f;
        scene->sunlightDirection[3] = 0.1f;

        swapchain_current_frame_unmap_buffer(FRAME_BUFFER_CAMERA);
}
