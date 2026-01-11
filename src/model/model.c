#include "model.h"

#include "common/array.h"
#include "common/str.h"
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

void MeshBuilderCreate(MeshBuilder *builder) {
        builder->positions = array(vec3);
        builder->normals = array(vec3);
        builder->uvs = array(TextureCoord);

        builder->vertices = array(Vertex);
        builder->indices = array(uint32_t);
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

typedef struct {
        uint32_t v[3];
        uint32_t vt[3];
        uint32_t vn[3];
} Face;

vec3 parse_vert(Str s) {
        vec3 r = {0};
        Cut c = make_cut(str_triml(s), ' ');
        r.x = str_to_float(c.head);
        c = make_cut(str_triml(c.tail), ' ');
        r.y = str_to_float(c.head);
        c = make_cut(str_triml(c.tail), ' ');
        r.z = str_to_float(c.head);

        return r;
}

vec3 parse_norm(Str s) {
        vec3 r = {0};
        Cut c = make_cut(str_triml(s), ' ');
        r.x = str_to_float(c.head);
        c = make_cut(str_triml(c.tail), ' ');
        r.y = str_to_float(c.head);
        c = make_cut(str_triml(c.tail), ' ');
        r.z = str_to_float(c.head);

        return r;
}

TextureCoord parse_tex(Str s) {
        TextureCoord r = {0};
        Cut c = make_cut(str_triml(s), ' ');
        r.u = str_to_float(c.head);
        c = make_cut(str_triml(c.tail), ' ');
        r.v = str_to_float(c.head);

        return r;
}

int parse_face(Str s, Face *f1, Face *f2) {
        int num_faces = 1;
        Cut fields = {0};
        fields.tail = s;

        *f1 = (Face){0};
        *f2 = (Face){0};

        for (int i = 0; i < 3; i += 1) {
                fields = make_cut(str_triml(fields.tail), ' ');
                Cut elem = make_cut(fields.head, '/');

                f1->v[i] = str_to_int(elem.head);

                elem = make_cut(elem.tail, '/');
                // vt? / vn?
                if (elem.head.len != 0) {
                        f1->vt[i] = str_to_int(elem.head);
                }

                if (elem.tail.len != 0) {
                        f1->vn[i] = str_to_int(elem.tail);
                }
        }

        fields = make_cut(str_triml(fields.tail), ' ');
        if (fields.head.len != 0) {
                f2->v[0] = f1->v[0];
                f2->v[1] = f1->v[2];

                f2->vt[0] = f1->vt[0];
                f2->vt[1] = f1->vt[2];

                f2->vn[0] = f1->vn[0];
                f2->vn[1] = f1->vn[2];

                num_faces = 2;
                Cut elem = make_cut(fields.head, '/');
                f2->v[2] = str_to_int(elem.head);

                elem = make_cut(elem.tail, '/');
                // vt? / vn?
                if (elem.head.len != 0) {
                        f2->vt[2] = str_to_int(elem.head);
                }

                if (elem.tail.len != 0) {
                        f2->vn[2] = str_to_int(elem.tail);
                }
        }

        return num_faces;
}

Model load_obj(const char *filename) {
        size_t filesize;
        char *data = ReadFile(filename, &filesize);
        if (!data) {
                printf("Failed to open file %s\n", filename);
                return (Model){0};
        }

        Str input = {data, filesize};
        Cut c = {0};
        c.tail = input;

        Model m = {0};
        m.meshes = malloc(sizeof(Mesh));
        m.meshes->vertices = array(Vertex);
        m.meshes->indices = array(uint32_t);
        m.num_meshes = 1;

        vec3 *positions = array(vec3);
        vec3 *normals = array(vec3);
        TextureCoord *texs = array(TextureCoord);
        Face *faces = array(Face);

        size_t num_lines = 0;
        while (c.tail.len) {
                c = make_cut(c.tail, '\n');
                Str line = c.head;

                Cut fields = make_cut(str_triml(line), ' ');
                if (str_equal(fields.head, make_str("v"))) {
                        array_append(positions, parse_vert(fields.tail));
                } else if (str_equal(fields.head, make_str("vn"))) {
                        array_append(normals, parse_norm(fields.tail));
                } else if (str_equal(fields.head, make_str("vt"))) {
                        array_append(texs, parse_tex(fields.tail));
                } else if (str_equal(fields.head, make_str("f"))) {
                        Face f1, f2;
                        int num_faces = parse_face(fields.tail, &f1, &f2);
                        array_append(faces, f1);
                        array_append(faces, f2);
                } else {
                        printf("unknown line: %.*s\n", fields.tail.len, fields.tail.data);
                }

                num_lines += 1;
        }

        printf("num lines: %zu\n", num_lines);

        for (int i = 0; i < array_length(faces); i += 1) {
                for (int f = 0; f < 3; f += 1) {
                        Vertex vert = {0};
                        uint32_t v = faces[i].v[f];
                        uint32_t vt = faces[i].vt[f];
                        uint32_t vn = faces[i].vn[f];

                        vert.position = positions[v - 1];
                        if (vn != 0) {
                                vert.normal = normals[vn - 1];
                        }
                        vert.color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
                        if (vt != 0) {
                                vert.uv_x = texs[vt - 1].u;
                                vert.uv_x = texs[vt - 1].v;
                        }

                        array_append(m.meshes->vertices, vert);
                        array_append(m.meshes->indices, array_length(m.meshes->indices));
                }
        }

        printf("Model Info:\n");
        for (int i = 0; i < m.num_meshes; i += 1) {
                printf("Mesh #%d\n:", i);
                printf("  Num Vertices: %zu\n", array_length(m.meshes->vertices));
                printf("  Num Indices: %zu\n", array_length(m.meshes->indices));
        }

        for (int i = 0; i < 3; i += 1) {
                Face f = faces[i];
                printf("f %d/%d/%d %d/%d/%d %d/%d/%d\n", f.v[0], f.vt[0], f.vn[0], f.v[1], f.vt[1],
                       f.vn[1], f.v[2], f.vt[2], f.vn[2]);
        }

        for (int i = 0; i < 3; i += 1) {
                vec3 pos = m.meshes->vertices[0].position;
                printf("face %d: {%f, %f, %f}\n", i, pos.x, pos.y, pos.z);
        }

        free(data);
        array_free(positions);
        array_free(normals);
        array_free(texs);
        array_free(faces);

        return m;
}

Model load_model(const char *filename) {
        int len = strlen(filename);

        if (strcmp(filename + (len - 4), ".obj") == 0) {
                return load_obj(filename);
        }

        printf("Could not determine filetype for file %s\n", filename);

        return (Model){0};
}

void model_destroy(Model m) {
        for (int i = 0; i < m.num_meshes; i += 1) {
                MeshFree(&m.meshes[i]);
        }

        free(m.meshes);
}
