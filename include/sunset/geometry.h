#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "sunset/backend.h"
#include "sunset/ecs.h"

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

struct Skeleton {};

struct SkeletonComponent {
  // NOTE: parents must come before their children
  std::vector<Bone> bones;
  std::vector<glm::mat4> final_transforms;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  glm::vec3 normal;
};

struct MeshRenderable {
  Handle vertex_buffer;
  size_t vertex_count;
  Handle index_buffer;
  size_t index_count;
  glm::vec3 normal;

  std::optional<Skeleton> skeleton;

  void serialize(std::ostream &os) const {}

  static MeshRenderable deserialize(std::istream &is) {
    return MeshRenderable{};
  }
};

struct AABB {
  glm::vec3 max;
  glm::vec3 min;

  bool intersects(AABB const &other) const;

  glm::vec3 getCenter() const;

  AABB extendTo() const;
};

struct Transform {
  // relative to parent
  glm::vec3 position;
  AABB bounding_box;
  glm::vec3 rotation;
  float scale = 1.0;
  std::vector<Entity> children;
  std::optional<Entity> parent;

  glm::mat4 cached_model;
  bool dirty;

  void serialize(std::ostream &os) const {}

  static Transform deserialize(std::istream &is) { return Transform{}; }
};

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity);

MeshRenderable compileMesh(Backend &backend, const Mesh &mesh);
