#include "model.h"

#include "image.h"

#include "common/array.h"
#include "common/log.h"
#include "common/str.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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

void material_info_destroy(MaterialInfo *mat) {
        if (mat->diffuse_tex)
                free(mat->diffuse_tex);
}

void process_mesh(const struct aiMesh *mesh, const struct aiScene *scene, Model *model) {
        Mesh my_mesh;
        my_mesh.vertices = array(Vertex);
        my_mesh.indices = array(uint32_t);

        for (int i = 0; i < mesh->mNumVertices; i++) {
                Vertex v;
                v.position[0] = mesh->mVertices[i].x;
                v.position[1] = mesh->mVertices[i].y;
                v.position[2] = mesh->mVertices[i].z;

                v.normal[0] = mesh->mNormals[i].x;
                v.normal[1] = mesh->mNormals[i].y;
                v.normal[2] = mesh->mNormals[i].z;

                v.uv_x = mesh->mTextureCoords[0][i].x;
                v.uv_y = mesh->mTextureCoords[0][i].y;

                if (mesh->mColors[0]) {
                        v.color[0] = mesh->mColors[0][i].r;
                        v.color[1] = mesh->mColors[0][i].g;
                        v.color[2] = mesh->mColors[0][i].b;
                        v.color[3] = mesh->mColors[0][i].a;
                } else {
                        for (int i = 0; i < 4; i++)
                                v.color[i] = 1.0f;
                }

                array_append(my_mesh.vertices, v);
        }

        for (int i = 0; i < mesh->mNumFaces; i++) {
                struct aiFace *face = &mesh->mFaces[i];
                if (face->mNumIndices != 3)
                        continue;

                array_append(my_mesh.indices, face->mIndices[0]);
                array_append(my_mesh.indices, face->mIndices[1]);
                array_append(my_mesh.indices, face->mIndices[2]);
        }

        if (mesh->mMaterialIndex >= 0) {
                my_mesh.material_index = mesh->mMaterialIndex;
        }

        array_append(model->meshes, my_mesh);
}

void process_node(const struct aiNode *node, const struct aiScene *scene, Model *model) {
        for (int i = 0; i < node->mNumMeshes; i++) {
                struct aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
                process_mesh(mesh, scene, model);
        }

        for (int i = 0; i < node->mNumChildren; i++) {
                process_node(node->mChildren[i], scene, model);
        }
}

void process_material(const struct aiMaterial *mat, Str dir, Model *model) {
        MaterialInfo info;

        info.ambient[0] = 1.0f;
        info.ambient[1] = 1.0f;
        info.ambient[2] = 1.0f;

        info.specular[0] = 0.5f;
        info.specular[1] = 0.5f;
        info.specular[2] = 0.5f;

        int num_diffuse = aiGetMaterialTextureCount(mat, aiTextureType_DIFFUSE);
        if (num_diffuse == 1) {
                struct aiString path;
                enum aiReturn r =
                    aiGetMaterialString(mat, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), &path);

                char *tex_path = malloc(dir.len + path.length + 2);
                memcpy(tex_path, dir.data, dir.len);
                tex_path[dir.len] = '/';
                memcpy(tex_path + dir.len + 1, path.data, path.length);
                tex_path[dir.len + path.length + 1] = '\0';

                int x, y, num_channels;
                stbi_set_flip_vertically_on_load(1);
                info.diffuse_tex = (char *)stbi_load(tex_path, &x, &y, &num_channels, 4);

                if (info.diffuse_tex == NULL) {
                        ERROR("failed to load image: %s", stbi_failure_reason());
                        exit(1);
                }

                info.diffuse_width = (size_t)x;
                info.diffuse_height = (size_t)y;

                free(tex_path);
        } else {
                DEBUG("Material Diffuse count: %d", num_diffuse);

                info.diffuse_tex = NULL;
        }

        array_append(model->materials, info);
}

Model load_model(char *filename) {
        Str path = str_span(filename, filename + strlen(filename));
        Str dir = make_cutr(path, '/').head;

        uint32_t flags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate;
        const struct aiScene *scene = aiImportFile(filename, flags);

        Model model;
        model.meshes = array(Mesh);
        model.materials = array(MaterialInfo);

        if (scene == NULL) {
                DEBUG("scene failed to load!");
        }
        process_node(scene->mRootNode, scene, &model);

        for (int i = 0; i < scene->mNumMaterials; i++) {
                process_material(scene->mMaterials[i], dir, &model);
        }

        aiReleaseImport(scene);

        return model;
}

void model_destroy(Model m) {
        for (int i = 0; i < array_length(m.meshes); i++) {
                MeshFree(&m.meshes[i]);
        }

        for (int i = 0; i < array_length(m.materials); i += 1) {
                material_info_destroy(&m.materials[i]);
        }

        array_free(m.meshes);
        array_free(m.materials);
}
