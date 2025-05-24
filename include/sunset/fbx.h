#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

#include "sunset/property_tree.h"

struct SavedMesh {
  std::string name;
  std::vector<float> vertices;
  std::vector<int32_t> indices;
  std::vector<float> uvs;
  std::vector<float> normals;
  int16_t material_id;
};

struct Texture {
  std::string name;
  std::string src;
};

struct Material {
  std::string name;
  int16_t texture_id;
};

struct Model {
  std::string name;
  std::vector<SavedMesh> meshes;
};

struct SavedScene {
  std::vector<Model> models;
  std::vector<Material> materials;
  std::vector<Texture> textures;
};

template <>
struct TypeDeserializer<Texture> {
  static std::vector<FieldDescriptor<Texture>> getFields() {
    return {
        makeSetter("Name", &Texture::name),
        makeSetter("Src", &Texture::src),
    };
  }
};

template <>
struct TypeDeserializer<Material> {
  static std::vector<FieldDescriptor<Material>> getFields() {
    return {
        makeSetter("Name", &Material::name),
        makeSetter("TextureId", &Material::texture_id),
    };
  }
};

template <>
struct TypeDeserializer<SavedMesh> {
  static std::vector<FieldDescriptor<SavedMesh>> getFields() {
    return {
        makeSetter("Name", &SavedMesh::name),
        makeSetter("Vertices", &SavedMesh::vertices, true),
        makeSetter("Indices", &SavedMesh::indices, true),
        makeSetter("UVs", &SavedMesh::uvs, true),
        makeSetter("Normals", &SavedMesh::normals, true),
        makeSetter("MaterialId", &SavedMesh::material_id, true),
    };
  }
};

template <>
struct TypeDeserializer<Model> {
  static std::vector<FieldDescriptor<Model>> getFields() {
    return {
        makeSetter("Name", &Model::name),
        makeSetter("Meshes", &Model::meshes, true),
    };
  }
};

template <>
struct TypeDeserializer<SavedScene> {
  static std::vector<FieldDescriptor<SavedScene>> getFields() {
    return {
        makeSetter("Models", &SavedScene::models, true),
        makeSetter("Materials", &SavedScene::materials, true),
        makeSetter("Textures", &SavedScene::textures, true),
    };
  }
};
