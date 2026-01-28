#include "camera.h"

#include "cglm/cglm.h"

Camera camera_default() {
        Camera r = {0};
        r.fov = 45.0f;

        // r.position = <0.0, 0.0, 2.0>
        r.position[2] = 2.0f;

        // r.target = <0.0, 0.0, -1.0>
        r.target[2] = -1.0f;

        // r.up = <0.0, 1.0, 0.0>
        r.up[1] = 1.0f;

        return r;
}

void camera_move(Camera *camera, vec3 delta) {
        // TODO: relative movement should be delta rotated by g_CameraViewDirection
        // vec3 relative = {delta.x, delta.y, -delta.z};
        glm_vec3_add(camera->position, delta, camera->position);
}

void camera_roate(Camera *camera, vec3 rot) {}
