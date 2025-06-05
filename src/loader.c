#include "loader.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        float u;
        float v;
} TextureCoord;

typedef struct {
        uint32_t count;
        uint32_t capacity;
        vec3 *data;
} vec3List;

bool vec3ListCreate(uint32_t capacity, vec3List *list) {
        list->capacity = capacity;
        list->count = 0;
        list->data = malloc(sizeof(vec3) * capacity);

        return list->data != NULL;
}

typedef struct {
        uint32_t count;
        uint32_t capacity;
        TextureCoord *data;
} TextureCoordList;

bool TextureCoordListCreate(uint32_t capacity, TextureCoordList *list) {
        list->capacity = capacity;
        list->count = 0;
        list->data = malloc(sizeof(TextureCoord) * capacity);

        return list->data != NULL;
}

bool ObjHandleVertexPosition(vec3List *p, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading vertex position %d\n", p->count);
                return false;
        }

        float x = strtof(x_str, NULL);
        float y = strtof(y_str, NULL);
        float z = strtof(z_str, NULL);

        if (p->count == p->capacity) {
                p->capacity *= 2;
                p->data = realloc(p->data, sizeof(vec3) * p->capacity);
        }

        p->data[p->count].x = x;
        p->data[p->count].y = y;
        p->data[p->count].z = z;

        p->count += 1;

        return true;
}

bool ObjHandleVertexNormals(vec3List *p, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading vertex position %d\n", p->count);
                return false;
        }

        float nx = strtof(x_str, NULL);
        float ny = strtof(y_str, NULL);
        float nz = strtof(z_str, NULL);

        if (p->count == p->capacity) {
                p->capacity *= 2;
                p->data = realloc(p->data, sizeof(vec3) * p->capacity);
        }

        p->data[p->count].x = nx;
        p->data[p->count].y = ny;
        p->data[p->count].z = nz;

        p->count += 1;

        return true;
}

Mesh *LoadObjFromFile(const char *filename) {
        FILE *f = fopen(filename, "rb");

        if (!f) {
                printf("Failed to open file %s\n", filename);
                return NULL;
        }

        fseek(f, 0, SEEK_END);
        size_t filesize = ftell(f);

        fseek(f, 0, SEEK_SET);
        char *data = malloc(filesize);

        fread(data, filesize, 1, f);
        fclose(f);

        char *save;
        char *line = strtok_r(data, "\n", &save);

        vec3List positions;
        vec3ListCreate(1000, &positions);

        vec3List normals;
        vec3ListCreate(1000, &normals);

        TextureCoordList uvs;
        TextureCoordListCreate(1000, &uvs);

        while (line) {
                char *line_save;
                char *token = strtok_r(line, " ", &line_save);

                if (!token) {
                        printf("Error parsing %s.\n", filename);

                        break;
                }

                if (strcmp(token, "v") == 0) {
                        ObjHandleVertexPosition(&positions, line_save);
                } else if (strcmp(token, "vn") == 0) {
                        ObjHandleVertexNormals(&normals, line_save);
                } else if (strcmp(token, "f") == 0) {
                        // handle faces
                }

                line = strtok_r(NULL, "\n", &save);
        }

        printf("num vertex positions: %d\n", positions.count);

        free(data);
        free(positions.data);
        free(normals.data);
        free(uvs.data);
        return NULL;
}

Mesh *LoadFromFile(const char *filename) {
        int len = strlen(filename);

        if (strcmp(filename + (len - 4), ".obj") == 0) {
                return LoadObjFromFile(filename);
        }

        printf("Could not determine filetype for file %s\n", filename);
        return NULL;
}
