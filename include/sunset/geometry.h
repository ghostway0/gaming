#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "sunset/backend.h"
#include "sunset/rman.h"
#include "sunset/ecs.h"
#include "sunset/property_tree.h"

struct AABB {
  glm::vec3 min;
  glm::vec3 max;

  bool intersects(AABB const &other) const;

  glm::vec3 getCenter() const;

  AABB extendTo(const glm::vec3 &pos) const;

  AABB subdivideIndex(size_t i, size_t total) const;

  bool contains(const glm::vec3 &point) const;

  float getRadius() const;

  AABB translate(const glm::vec3 &direction) const;

  AABB rotate(const glm::quat &rotation) const;

  AABB scale(float rotation) const;
};

template <>
struct TypeDeserializer<AABB> {
  static std::vector<FieldDescriptor<AABB>> getFields() {
    return {
        makeSetter("Min", &AABB::min, true),
        makeSetter("Max", &AABB::max, true),
    };
  }
};

std::ostream &operator<<(std::ostream &os, const AABB &aabb);

struct Rect {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::ivec4 bone_ids;
  glm::vec4 bone_weights;
};

struct Bone {
  std::optional<size_t> parent_index;
  glm::mat4 inverse_bind_matrix;
  glm::mat4 local_transform;
};

struct Skeleton {
  // NOTE: parents must come before their children
  std::vector<Bone> bones;
  std::vector<glm::mat4> final_transforms;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  glm::vec3 normal;
  std::optional<Skeleton> skeleton;
  AABB bounding_box;
};

struct MeshRef {
  RRef rref;

  std::optional<PropertyTree> serialize() const { return std::nullopt; }

  static absl::StatusOr<MeshRef> deserialize(PropertyTree const &tree) {
    return MeshRef{TRY(deserializeTree<RRef>(tree))};
  }
};

struct TextureRef {
  RRef rref;

  std::optional<PropertyTree> serialize() const { return std::nullopt; }

  static absl::StatusOr<TextureRef> deserialize(PropertyTree const &tree) {
    return TextureRef{TRY(deserializeTree<RRef>(tree))};
  }
};

struct MeshRenderable {
  Handle vertex_buffer;
  size_t vertex_count;
  Handle index_buffer;
  size_t index_count;
  glm::vec3 normal;
  std::optional<Handle> texture;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"MeshRef"};
    return tree;
  }

  static absl::StatusOr<MeshRenderable> deserialize(
      PropertyTree const & /* tree */) {
    return absl::InternalError("Cached structures should not be saved.");
  }
};

struct SavedMesh {
  std::vector<float> vertices;
  std::vector<int32_t> indices;
  std::vector<float> uvs;
  std::vector<float> normals;
  int16_t material_id;
};

template <>
struct TypeDeserializer<SavedMesh> {
  static std::vector<FieldDescriptor<SavedMesh>> getFields() {
    return {
        makeSetter("Vertices", &SavedMesh::vertices, true),
        makeSetter("Indices", &SavedMesh::indices, true),
        makeSetter("UVs", &SavedMesh::uvs, true),
        makeSetter("Normals", &SavedMesh::normals, true),
        makeSetter("MaterialId", &SavedMesh::material_id, true),
    };
  }
};

struct Texture {
  std::string src;
};

struct Material {
  std::string name;
  int16_t texture_id;
};

struct Model {
  std::vector<SavedMesh> meshes;
};

template <>
struct TypeDeserializer<glm::vec4> {
  static std::vector<FieldDescriptor<glm::vec4>> getFields() {
    return {
        makeSetter("x", &glm::vec4::x),
        makeSetter("y", &glm::vec4::y),
        makeSetter("z", &glm::vec4::z),
        makeSetter("w", &glm::vec4::w),
    };
  }
};

template <>
struct TypeDeserializer<glm::quat> {
  static std::vector<FieldDescriptor<glm::quat>> getFields() {
    return {
        makeSetter("x", &glm::quat::x),
        makeSetter("y", &glm::quat::y),
        makeSetter("z", &glm::quat::z),
        makeSetter("w", &glm::quat::w),
    };
  }
};

template <>
struct TypeDeserializer<glm::vec3> {
  static std::vector<FieldDescriptor<glm::vec3>> getFields() {
    return {
        makeSetter("x", &glm::vec3::x),
        makeSetter("y", &glm::vec3::y),
        makeSetter("z", &glm::vec3::z),
    };
  }
};

struct SavedTransform {
  glm::vec3 position;
  glm::quat rotation;
  float scale;
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

struct Id {
  std::string id;
};

template <>
struct TypeDeserializer<Id> {
  static std::vector<FieldDescriptor<Id>> getFields() {
    return {
        makeSetter("Id", &Id::id, true),
    };
  }
};

template <>
struct TypeDeserializer<Texture> {
  static std::vector<FieldDescriptor<Texture>> getFields() {
    return {
        makeSetter("Src", &Texture::src, true),
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
struct TypeDeserializer<Model> {
  static std::vector<FieldDescriptor<Model>> getFields() {
    return {
        makeSetter("Meshes", &Model::meshes, true),
    };
  }
};

struct Instance {
  std::vector<PropertyTree> components;
};

template <>
struct TypeDeserializer<Instance> {
  static std::vector<FieldDescriptor<Instance>> getFields() {
    return {
        makeSetter("Components", &Instance::components),
    };
  }
};

struct SavedScene {
  std::string scope;
  std::vector<PropertyTree> resources;
  std::vector<Instance> instances;
};

template <>
struct TypeDeserializer<SavedScene> {
  static std::vector<FieldDescriptor<SavedScene>> getFields() {
    return {
        makeSetter("Scope", &SavedScene::scope, true),
        makeSetter("Resources", &SavedScene::resources, true),
        makeSetter("Instances", &SavedScene::instances, true),
    };
  }
};

absl::StatusOr<MeshRenderable> loadSavedMesh(
    MeshRef const &ref, SavedMesh const &saved_mesh,
    std::optional<std::string> texture_path, Backend &backend);

struct Transform {
  // relative to parent
  glm::vec3 position;
  glm::quat rotation;
  float scale = 1.0;
  // FIXME: If scenes are loaded from multiple instantiations
  // of an ECS world, indices may be incorrect. A solution
  // would be to use a unique id based on the content, and have
  // an EntityRegistry struct that is passed on deserialization.
  std::vector<Entity> children;
  std::optional<Entity> parent;

  glm::mat4 cached_model;
  bool dirty;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    return PropertyTree();
  }

  static absl::StatusOr<Transform> deserialize(PropertyTree const &tree) {
    SavedTransform t = TRY(deserializeTree<SavedTransform>(tree));
    return Transform{
        t.position,  t.rotation, static_cast<float>(t.scale), {}, {},
        glm::mat4(), true,
    };
  }
};

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity);

MeshRenderable compileMesh(
    Backend &backend, const Mesh &mesh,
    std::optional<Image> texture_image = std::nullopt);

void rotateEntity(ECS &ecs, Entity e);

namespace glm {

std::ostream &operator<<(std::ostream &os, const vec3 &vec);
std::ostream &operator<<(std::ostream &os, const vec4 &vec);
std::ostream &operator<<(std::ostream &os, const quat &vec);

} // namespace glm
