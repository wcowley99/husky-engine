#include "loader.h"

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const size_t DEFAULT_CAPACITY = 64;

typedef struct {
        vec3List positions;
        vec3List normals;
        TextureCoordList uvs;

        VertexList vertices;
        uint32_tList indices;
} MeshBuilder;

IMPL_LIST_OPERATIONS(TextureCoord)
IMPL_LIST_OPERATIONS(Vertex)

bool MeshBuilderCreate(MeshBuilder *builder) {
        builder->positions = vec3ListCreate(DEFAULT_CAPACITY);
        builder->normals = vec3ListCreate(DEFAULT_CAPACITY);
        builder->uvs = TextureCoordListCreate(DEFAULT_CAPACITY);

        builder->vertices = VertexListCreate(DEFAULT_CAPACITY);
        builder->indices = uint32_tListCreate(DEFAULT_CAPACITY);

        return true;
}

void MeshBuilderFree(MeshBuilder *builder) {
        vec3ListFree(&builder->positions);
        vec3ListFree(&builder->normals);
        TextureCoordListFree(&builder->uvs);
}

Mesh MeshBuilderBuild(MeshBuilder *b) {
        Vertex *vertices = realloc(b->vertices.data, sizeof(Vertex) * b->vertices.length);
        uint32_t *indices = realloc(b->indices.data, sizeof(uint32_t) * b->indices.length);

        return (Mesh){
            .vertices = vertices,
            .num_vertices = b->vertices.length,
            .indices = indices,
            .num_indices = b->indices.length,
        };
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
                printf("Error reading vertex position %lu\n", b->positions.length);
                return false;
        }

        float x = strtof(x_str, NULL);
        float y = strtof(y_str, NULL);
        float z = strtof(z_str, NULL);

        vec3 *pos = vec3ListReserve(&b->positions);
        pos->x = x;
        pos->y = y;
        pos->z = z;

        return true;
}

bool ObjHandleVertexNormals(MeshBuilder *b, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading normal position %lu\n", b->normals.length);
                return false;
        }

        float nx = strtof(x_str, NULL);
        float ny = strtof(y_str, NULL);
        float nz = strtof(z_str, NULL);

        vec3 *normal = vec3ListReserve(&b->normals);
        normal->x = nx;
        normal->y = ny;
        normal->z = nz;

        return true;
}

bool ObjHandleVertexTextureCoords(MeshBuilder *b, char *line_save) {
        char *u_str = strtok_r(NULL, " ", &line_save);
        char *v_str = strtok_r(NULL, " ", &line_save);

        if (!u_str || !v_str) {
                printf("Error reading vertex texture coord %lu\n", b->uvs.length);
                return false;
        }

        float u = strtof(u_str, NULL);
        float v = strtof(v_str, NULL);

        TextureCoordListPush(&b->uvs, (TextureCoord){.u = u, .v = v});

        return true;
}

bool ObjHandleVertex(MeshBuilder *b, char *contents) {
        Vertex *vertex = VertexListReserve(&b->vertices);
        vertex->color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
        char *next = strtok_r(NULL, "/", &contents);

        if (!next) {
                printf("Error reading vertex position index.\n");
                return false;
        }
        uint32_t pos = strtoul(next, NULL, 10) - 1;
        vertex->position = *vec3ListGet(&b->positions, pos);

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t uv = strtoul(next, NULL, 10) - 1;
                TextureCoord *coord = TextureCoordListGet(&b->uvs, uv);
                vertex->uv_x = coord->u;
                vertex->uv_y = coord->v;
        }

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t vn = strtoul(next, NULL, 10) - 1;
                vertex->normal = *vec3ListGet(&b->normals, vn);
        }

        return true;
}

bool ObjHandleFaces(MeshBuilder *b, char *line_save) {
        for (uint32_t i = 0; i < 3; i += 1) {
                char *vertex = strtok_r(NULL, " ", &line_save);

                ObjHandleVertex(b, vertex);
                uint32_tListPush(&b->indices, b->indices.length);
        }

        return true;
}

bool LoadObjFromFile(Mesh *mesh, const char *filename) {
        size_t filesize;
        char *data = ReadFile(filename, &filesize);
        if (!data) {
                printf("Failed to open file %s\n", filename);
                return false;
        }

        MeshBuilder builder;
        MeshBuilderCreate(&builder);

        char *save;
        char *line = strtok_r(data, "\n", &save);

        while (line) {
                char *line_save;
                char *token = strtok_r(line, " ", &line_save);

                if (!token) {
                        printf("Error parsing %s.\n", filename);
                        free(data);
                        return false;
                }

                if (strcmp(token, "v") == 0) {
                        ObjHandleVertexPosition(&builder, line_save);
                } else if (strcmp(token, "vn") == 0) {
                        ObjHandleVertexNormals(&builder, line_save);
                } else if (strcmp(token, "vt") == 0) {
                        ObjHandleVertexTextureCoords(&builder, line_save);
                } else if (strcmp(token, "f") == 0) {
                        ObjHandleFaces(&builder, line_save);
                } else if (token[0] != '#') {
                        // This line is not a comment, print token for debugging purposes
                        printf("Unknown token for line \"%s\"\n", token);
                }

                line = strtok_r(NULL, "\n", &save);
        }

        free(data);
        *mesh = MeshBuilderBuild(&builder);

        MeshBuilderFree(&builder);

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
