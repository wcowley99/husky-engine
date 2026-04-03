#pragma once

#include <cglm/cglm.h>

typedef struct {
        vec3 position;
        vec3 target;
        vec3 up;

        float fov;
} Camera;

void renderer_set_camera(Camera camera);
