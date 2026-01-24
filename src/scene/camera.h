#pragma once

#include "common/linalgebra.h"

typedef struct {
        vec3 position;
        vec3 target;
        vec3 up;

        float fov;
} Camera;

void camera_move(Camera *camera, vec3 delta);
