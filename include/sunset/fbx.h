#include <string>
#include <vector>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

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

struct Vector3 {
  double x, y, z;

  inline glm::vec3 glm() const { return glm::vec3(x, y, z); }
};

template <>
struct TypeDeserializer<Vector3> {
  static std::vector<FieldDescriptor<Vector3>> getFields() {
    return {
        makeSetter("x", &Vector3::x),
        makeSetter("y", &Vector3::y),
        makeSetter("z", &Vector3::z),
    };
  }
};

struct Vector4 {
  double x, y, z, w;

  inline glm::vec4 glm() const { return glm::vec4(x, y, z, w); }
  
  inline glm::quat quat() const { return glm::quat(w, x, y, z); }
};

template <>
struct TypeDeserializer<Vector4> {
  static std::vector<FieldDescriptor<Vector4>> getFields() {
    return {
        makeSetter("x", &Vector4::x),
        makeSetter("y", &Vector4::y),
        makeSetter("z", &Vector4::z),
        makeSetter("w", &Vector4::w),
    };
  }
};

struct SavedTransform {
  Vector3 position;
  Vector4 rotation;
  double scale;
};

template <>
struct TypeDeserializer<SavedTransform> {
  static std::vector<FieldDescriptor<SavedTransform>> getFields() {
    return {
        makeSetter("Position", &SavedTransform::position, true),
        makeSetter("Rotation", &SavedTransform::rotation, true),
        makeSetter("Scale", &SavedTransform::scale, true),
    };
  }
};

struct Instance {
  int16_t physics_type;
  SavedTransform transform;
  int16_t model_id;
};

template <>
struct TypeDeserializer<Instance> {
  static std::vector<FieldDescriptor<Instance>> getFields() {
    return {
        makeSetter("Transform", &Instance::transform),
        makeSetter("ModelId", &Instance::model_id, true),
        makeSetter("PhysicsType", &Instance::physics_type, true),
    };
  }
};

struct SavedScene {
  std::vector<Model> models;
  std::vector<Material> materials;
  std::vector<Texture> textures;
  std::vector<Instance> instances;
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
        makeSetter("Instances", &SavedScene::instances, true),
    };
  }
};
