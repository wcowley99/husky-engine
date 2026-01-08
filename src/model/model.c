#include "model.h"

#include "common/array.h"
#include "common/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        vec3 *positions;
        vec3 *normals;
        TextureCoord *uvs;

        Vertex *vertices;
        uint32_t *indices;
} MeshBuilder;

bool MeshBuilderCreate(MeshBuilder *builder) {
        builder->positions = array(vec3);
        builder->normals = array(vec3);
        builder->uvs = array(TextureCoord);

        builder->vertices = array(Vertex);
        builder->indices = array(uint32_t);

        return true;
}

void MeshBuilderFree(MeshBuilder *builder) {
        array_free(builder->positions);
        array_free(builder->normals);
        array_free(builder->uvs);
}

Mesh MeshBuilderBuild(MeshBuilder *b) {
        return (Mesh){
            .vertices = b->vertices,
            .indices = b->indices,
        };
}

void MeshFree(Mesh *mesh) {
        array_free(mesh->vertices);
        array_free(mesh->indices);
}

bool ObjHandleVertexPosition(MeshBuilder *b, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading vertex position %lu\n", array_length(b->positions));
                return false;
        }

        float x = strtof(x_str, NULL);
        float y = strtof(y_str, NULL);
        float z = strtof(z_str, NULL);

        vec3 pos = (vec3){x, y, z};
        array_append(b->positions, pos);

        return true;
}

bool ObjHandleVertexNormals(MeshBuilder *b, char *line_save) {
        char *x_str = strtok_r(NULL, " ", &line_save);
        char *y_str = strtok_r(NULL, " ", &line_save);
        char *z_str = strtok_r(NULL, " ", &line_save);

        if (!x_str || !y_str || !z_str) {
                printf("Error reading normal position %lu\n", array_length(b->normals));
                return false;
        }

        float nx = strtof(x_str, NULL);
        float ny = strtof(y_str, NULL);
        float nz = strtof(z_str, NULL);

        vec3 normal = (vec3){nx, ny, nz};
        array_append(b->normals, normal);

        return true;
}

bool ObjHandleVertexTextureCoords(MeshBuilder *b, char *line_save) {
        char *u_str = strtok_r(NULL, " ", &line_save);
        char *v_str = strtok_r(NULL, " ", &line_save);

        if (!u_str || !v_str) {
                printf("Error reading vertex texture coord %lu\n", array_length(b->uvs));
                return false;
        }

        float u = strtof(u_str, NULL);
        float v = strtof(v_str, NULL);

        TextureCoord tex = (TextureCoord){.u = u, .v = v};
        array_append(b->uvs, tex);

        return true;
}

bool ObjHandleVertex(MeshBuilder *b, char *contents) {
        Vertex v = {0};
        v.color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
        char *next = strtok_r(NULL, "/", &contents);

        if (!next) {
                printf("Error reading vertex position index.\n");
                return false;
        }
        uint32_t pos = strtoul(next, NULL, 10) - 1;
        v.position = b->positions[pos];

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t uv = strtoul(next, NULL, 10) - 1;
                TextureCoord *coord = &b->uvs[uv];
                v.uv_x = coord->u;
                v.uv_y = coord->v;
        }

        next = strtok_r(NULL, "/", &contents);
        if (next && next[0] != '\0') {
                uint32_t vn = strtoul(next, NULL, 10) - 1;
                v.normal = b->normals[vn];
        }

        array_append(b->vertices, v);

        return true;
}

bool ObjHandleFaces(MeshBuilder *b, char *line_save) {
        for (uint32_t i = 0; i < 3; i += 1) {
                char *vertex = strtok_r(NULL, " ", &line_save);

                ObjHandleVertex(b, vertex);
                array_append(b->indices, array_length(b->indices));
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
