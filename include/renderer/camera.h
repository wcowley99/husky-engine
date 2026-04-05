#pragma once

#include <cglm/cglm.h>

typedef struct camera {
        vec3 position;
        vec3 target;
        vec3 up;

        float fov;
} camera_t;

void renderer_set_camera(camera_t camera);
