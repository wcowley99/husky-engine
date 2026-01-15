#include "model.h"

#include "common/array.h"
#include "common/str.h"
#include "common/util.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        uint32_t v[3];
        uint32_t vt[3];
        uint32_t vn[3];
} Face;

void MeshFree(Mesh *mesh) {
        array_free(mesh->vertices);
        array_free(mesh->indices);
}

void material_info_destroy(MaterialInfo *mat) { free(mat->diffuse_tex); }

vec3 parse_vec3(Str s) {
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

void parse_face(Str s, Face **faces) {
        Cut fields = {0};
        fields.tail = s;

        Face f = {0};

        for (int i = 0; i < 3; i += 1) {
                fields = make_cut(str_triml(fields.tail), ' ');
                Cut elem = make_cut(fields.head, '/');

                f.v[i] = str_to_int(elem.head);

                elem = make_cut(elem.tail, '/');
                // vt? / vn?
                if (elem.head.len != 0) {
                        f.vt[i] = str_to_int(elem.head);
                }

                if (elem.tail.len != 0) {
                        f.vn[i] = str_to_int(elem.tail);
                }
        }

        array_append(*faces, f);

        fields = make_cut(str_triml(fields.tail), ' ');
        if (fields.head.len != 0) {
                f.v[1] = f.v[2];
                f.vt[1] = f.vt[2];
                f.vn[1] = f.vn[2];

                Cut elem = make_cut(fields.head, '/');
                f.v[2] = str_to_int(elem.head);

                elem = make_cut(elem.tail, '/');
                // vt? / vn?
                if (elem.head.len != 0) {
                        f.vt[2] = str_to_int(elem.head);
                }

                if (elem.tail.len != 0) {
                        f.vn[2] = str_to_int(elem.tail);
                }
        }

        array_append(*faces, f);
}

MaterialInfo *load_mats(Str path, Str mtlfile) {
        char *filepath = malloc(path.len + mtlfile.len + 2);
        memcpy(filepath, path.data, path.len);
        filepath[path.len] = '/';
        memcpy(filepath + path.len + 1, mtlfile.data, mtlfile.len);
        filepath[path.len + mtlfile.len + 1] = '\0';

        size_t filesize;
        char *data = ReadFile(filepath, &filesize);

        free(filepath);

        Str input = {data, filesize};
        Cut c = {0};
        c.tail = input;

        MaterialInfo *mats = array(MaterialInfo);

        MaterialInfo *current = NULL;

        while (c.tail.len) {
                c = make_cut(c.tail, '\n');
                Str line = c.head;

                Cut fields = make_cut(str_triml(line), ' ');
                if (str_equal(fields.head, make_str("newmtl"))) {
                        array_append(mats, (MaterialInfo){0});
                        current = &mats[array_length(mats) - 1];
                        current->name = str_to_chars(fields.tail);
                } else if (!fields.head.len || !current) {
                        continue;
                } else if (str_equal(fields.head, make_str("Ka"))) {
                        current->ambient = parse_vec3(fields.tail);
                } else if (str_equal(fields.head, make_str("Ks"))) {
                        current->specular = parse_vec3(fields.tail);
                } else if (str_equal(fields.head, make_str("map_Kd"))) {
                        char *texpath = malloc(path.len + fields.tail.len + 2);
                        memcpy(texpath, path.data, path.len);
                        texpath[path.len] = '/';
                        memcpy(texpath + path.len + 1, fields.tail.data, fields.tail.len);
                        texpath[path.len + fields.tail.len + 1] = '\0';

                        int width, height, channels;
                        stbi_set_flip_vertically_on_load(true);
                        current->diffuse_tex =
                            (char *)stbi_load(texpath, &width, &height, &channels, 4);
                        printf("num_channels: %d\n", channels);
                        current->diffuse_width = width;
                        current->diffuse_height = height;

                        free(texpath);
                } else if (fields.head.data[0] != '#') {
                        printf("unknown .mtl line leader: %.*s\n", fields.head.len,
                               fields.head.data);
                }
        }

        return mats;
}

Model load_obj(const char *filename) {
        Str file = {filename, strlen(filename)};
        Cut path = make_cutr(file, '/');

        printf("path: %s\n", filename);
        printf("head: %.*s | tail: %.*s\n", path.head.len, path.head.data, path.tail.len,
               path.tail.data);

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
        m.meshes = array(Mesh);
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
                if (!fields.head.len) {
                        continue;
                } else if (str_equal(fields.head, make_str("mtllib"))) {
                        m.materials = load_mats(path.head, fields.tail);
                } else if (str_equal(fields.head, make_str("v"))) {
                        array_append(positions, parse_vec3(fields.tail));
                } else if (str_equal(fields.head, make_str("vn"))) {
                        array_append(normals, parse_vec3(fields.tail));
                } else if (str_equal(fields.head, make_str("vt"))) {
                        array_append(texs, parse_tex(fields.tail));
                } else if (str_equal(fields.head, make_str("f"))) {
                        parse_face(fields.tail, &faces);
                } else if (str_equal(fields.head, make_str("usemtl"))) {
                        printf("todo: set material\n");
                } else if (fields.head.data[0] != '#') {
                        // not a comment, unknown field
                        printf("unknown .obj line leader: %.*s\n", fields.head.len,
                               fields.head.data);
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

                        // OBJ models are 1-indexed, so to convert to the proper array
                        // index, we need to subtract 1
                        vert.position = positions[v - 1];
                        if (vn != 0) {
                                vert.normal = normals[vn - 1];
                        }
                        vert.color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
                        if (vt != 0) {
                                vert.uv_x = texs[vt - 1].u;
                                vert.uv_y = texs[vt - 1].v;
                        }

                        // TODO: refactor to actually reuse indices
                        array_append(m.meshes->vertices, vert);
                        array_append(m.meshes->indices, array_length(m.meshes->indices));
                }
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

        for (int i = 0; i < array_length(m.materials); i += 1) {
                material_info_destroy(&m.materials[i]);
        }

        array_free(m.meshes);
        array_free(m.materials);
}
