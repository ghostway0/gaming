#pragma once

#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "sunset/backend.h"
#include "sunset/ecs.h"

struct AABB {
  glm::vec3 min;
  glm::vec3 max;

  bool intersects(AABB const &other) const;

  glm::vec3 getCenter() const;

  AABB extendTo(const glm::vec3 &pos) const;

  AABB subdivideIndex(size_t i, size_t total) const;

  bool contains(const glm::vec3 &point) const;

  float getRadius() const;

  AABB translate(const glm::vec3 &direction);
};

std::ostream& operator<<(std::ostream& os, const AABB& aabb);

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

struct MeshRenderable {
  Handle vertex_buffer;
  size_t vertex_count;
  Handle index_buffer;
  size_t index_count;
  glm::vec3 normal;

  void serialize(std::ostream &os) const {}

  static MeshRenderable deserialize(std::istream &is) {
    return MeshRenderable{};
  }
};

struct Transform {
  // relative to parent
  glm::vec3 position;
  AABB bounding_box;
  glm::quat rotation;
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
