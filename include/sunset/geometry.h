#pragma once

#include <vector>

#include <glm/glm.hpp>

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

class AABB {
 public:
  AABB(glm::vec3 max, glm::vec3 min);

 private:
  glm::vec3 max_;
  glm::vec3 min_;
};
