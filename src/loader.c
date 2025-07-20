#include "loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint32_t DEFAULT_CAPACITY = 64;

typedef struct {
        float u;
        float v;
} TextureCoord;

typedef struct {
        vec3 *positions;
        uint32_t pos_count;
        uint32_t pos_capacity;

        vec3 *normals;
        uint32_t normal_count;
        uint32_t normal_capacity;

        TextureCoord *uvs;
        uint32_t uv_count;
        uint32_t uv_capacity;

        Mesh mesh;
        uint32_t vertex_capacity;
        uint32_t index_capacity;
} MeshBuilder;

bool MeshBuilderCreate(MeshBuilder *builder) {
        builder->pos_count = 0;
        builder->pos_capacity = DEFAULT_CAPACITY;
        builder->positions = malloc(sizeof(vec3) * builder->pos_capacity);

        builder->normal_count = 0;
        builder->normal_capacity = DEFAULT_CAPACITY;
        builder->normals = malloc(sizeof(vec3) * builder->pos_capacity);

        builder->uv_count = 0;
        builder->uv_capacity = DEFAULT_CAPACITY;
        builder->uvs = malloc(sizeof(TextureCoord) * builder->pos_capacity);

        builder->vertex_capacity = DEFAULT_CAPACITY;
        builder->mesh.num_vertices = 0;
        builder->mesh.vertices = malloc(sizeof(Vertex) * builder->vertex_capacity);

        builder->index_capacity = DEFAULT_CAPACITY;
        builder->mesh.num_indices = 0;
        builder->mesh.indices = malloc(sizeof(Vertex) * builder->index_capacity);

        return true;
}

void MeshBuilderFree(MeshBuilder *builder) {
        free(builder->positions);
        free(builder->normals);
        free(builder->uvs);
}

void MeshFree(Mesh *mesh) {
        free(mesh->indices);
        free(mesh->vertices);
}

bool ObjHandleVertexPosition(MeshBuilder *b, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading vertex position %d\n", b->pos_count);
                return false;
        }

        float x = strtof(x_str, NULL);
        float y = strtof(y_str, NULL);
        float z = strtof(z_str, NULL);

        if (b->pos_count == b->pos_capacity) {
                b->pos_capacity *= 2;
                b->positions = realloc(b->positions, sizeof(vec3) * b->pos_capacity);
        }

        b->positions[b->pos_count].x = x;
        b->positions[b->pos_count].y = y;
        b->positions[b->pos_count].z = z;

        b->pos_count += 1;

        return true;
}

bool ObjHandleVertexNormals(MeshBuilder *b, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading vertex position %d\n", b->normal_count);
                return false;
        }

        float nx = strtof(x_str, NULL);
        float ny = strtof(y_str, NULL);
        float nz = strtof(z_str, NULL);

        if (b->normal_count == b->normal_capacity) {
                b->normal_capacity *= 2;
                b->normals = realloc(b->normals, sizeof(vec3) * b->normal_capacity);
        }

        b->normals[b->normal_count].x = nx;
        b->normals[b->normal_count].y = ny;
        b->normals[b->normal_count].z = nz;

        b->normal_count += 1;

        return true;
}

bool ObjHandleVertex(MeshBuilder *b, char *contents) {
        Vertex *vertex = &b->mesh.vertices[b->mesh.num_vertices];
        vertex->color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
        char *next = strtok_r(NULL, "/", &contents);

        if (!next) {
                printf("Error reading vertex position index.\n");
                return false;
        }
        uint32_t pos = strtoul(next, NULL, 10) - 1;
        vertex->position = b->positions[pos];

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t uv = strtoul(next, NULL, 10) - 1;
                vertex->uv_x = b->uvs[uv].u;
                vertex->uv_y = b->uvs[uv].v;
        }

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t vn = strtoul(next, NULL, 10) - 1;
                vertex->normal = b->normals[vn];
        }

        return true;
}

bool ObjHandleFaces(MeshBuilder *b, char *line_save) {
        if (b->mesh.num_vertices + 3 >= b->vertex_capacity) {
                b->vertex_capacity *= 2;
                b->mesh.vertices = realloc(b->mesh.vertices, sizeof(Vertex) * b->vertex_capacity);
        }

        if (b->mesh.num_indices + 3 >= b->index_capacity) {
                b->index_capacity *= 2;
                b->mesh.indices = realloc(b->mesh.indices, sizeof(uint32_t) * b->vertex_capacity);
        }

        for (uint32_t i = 0; i < 3; i += 1) {
                char *vertex = strtok_r(NULL, " ", &line_save);

                ObjHandleVertex(b, vertex);
                b->mesh.num_vertices += 1;

                b->mesh.indices[b->mesh.num_indices] = b->mesh.num_indices;
                b->mesh.num_indices += 1;
        }

        return true;
}

bool LoadObjFromFile(Mesh *mesh, const char *filename) {
        FILE *f = fopen(filename, "rb");

        if (!f) {
                printf("Failed to open file %s\n", filename);
                return false;
        }

        fseek(f, 0, SEEK_END);
        size_t filesize = ftell(f);

        fseek(f, 0, SEEK_SET);
        char *data = malloc(filesize);

        fread(data, filesize, 1, f);
        fclose(f);

        MeshBuilder builder;
        MeshBuilderCreate(&builder);

        char *save;
        char *line = strtok_r(data, "\n", &save);

        while (line) {
                char *line_save;
                char *token = strtok_r(line, " ", &line_save);

                if (!token) {
                        printf("Error parsing %s.\n", filename);

                        return false;
                }

                if (strcmp(token, "v") == 0) {
                        ObjHandleVertexPosition(&builder, line_save);
                } else if (strcmp(token, "vn") == 0) {
                        ObjHandleVertexNormals(&builder, line_save);
                } else if (strcmp(token, "vt") == 0) {
                        // todo: uvs
                } else if (strcmp(token, "f") == 0) {
                        ObjHandleFaces(&builder, line_save);
                } else {
                        printf("Unknown token for line \"%s\"\n", token);
                }

                line = strtok_r(NULL, "\n", &save);
        }

        printf("num vertex positions: %d\n", builder.pos_count);

        free(data);
        MeshBuilderFree(&builder);
        *mesh = builder.mesh;

        return true;
}

bool LoadFromFile(Mesh *mesh, const char *filename) {
        int len = strlen(filename);

        if (strcmp(filename + (len - 4), ".obj") == 0) {
                return LoadObjFromFile(mesh, filename);
        }

        printf("Could not determine filetype for file %s\n", filename);
        return false;
}
