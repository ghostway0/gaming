#pragma once

#include <vector>

#include <glm/glm.hpp>
#include "sunset/backend.h"
#include "sunset/ecs.h"

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
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

  void serialize(std::ostream &os) const {}

  static MeshRenderable deserialize(std::istream &is) {
    return MeshRenderable{};
  }
};

class AABB {
 public:
  AABB(glm::vec3 max, glm::vec3 min);
  
  AABB() : max_{}, min_{} {}

 private:
  glm::vec3 max_;
  glm::vec3 min_;
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
};

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity);

MeshRenderable compileMesh(Backend &backend, const Mesh &mesh);
