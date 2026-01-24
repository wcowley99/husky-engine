#include "camera.h"

void camera_move(Camera *camera, vec3 delta) {
        // TODO: relative movement should be delta rotated by g_CameraViewDirection
        // vec3 relative = {delta.x, delta.y, -delta.z};
        camera->position = vec3_add(camera->position, delta);
}
