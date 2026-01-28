#pragma once

#include "cglm/cglm.h"

typedef struct {
        vec3 position;
        vec3 target;
        vec3 up;

        float fov;
} Camera;

Camera camera_default();

void camera_move(Camera *camera, vec3 delta);
void camera_roate(Camera *camera, vec3 rot);
