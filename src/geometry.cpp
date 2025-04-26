#include <cassert>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "sunset/utils.h"

#include "sunset/geometry.h"

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity) {
  glm::mat4 model_matrix = glm::mat4(1.0f);

  Entity current = entity;

  while (true) {
    const Transform *transform = ecs.getComponent<Transform>(current);

    glm::mat4 local = glm::mat4(1.0f);

    local *= glm::toMat4(transform->rotation);

    local = glm::scale(local, glm::vec3(transform->scale));

    local = glm::translate(local, transform->position);

    model_matrix = local * model_matrix;

    if (!transform->parent.has_value()) {
      break;
    }

    current = transform->parent.value();
  }

  return model_matrix;
}

MeshRenderable compileMesh(Backend &backend, const Mesh &mesh) {
  return MeshRenderable{
      .vertex_buffer = backend.upload(to_bytes_view(mesh.vertices)),
      .vertex_count = mesh.vertices.size(),
      .index_buffer = backend.upload(to_bytes_view(mesh.indices)),
      .index_count = mesh.indices.size(),
      .normal = mesh.normal,
  };
}

bool AABB::intersects(const AABB &other) const {
  return (min.x <= other.max.x && max.x >= other.min.x) &&
         (min.y <= other.max.y && max.y >= other.min.y) &&
         (min.z <= other.max.z && max.z >= other.min.z);
}

glm::vec3 AABB::getCenter() const {
  return (min + max) * 0.5f;
}

AABB AABB::extendTo(const glm::vec3 &pos) const {
  return {glm::min(min, pos), glm::max(max, pos)};
}

AABB AABB::subdivideIndex(size_t i, size_t total) const {
  assert(i < total);

  size_t n = static_cast<size_t>(std::cbrt(static_cast<double>(total)));
  if (n * n * n < total) {
    ++n;
  }

  glm::vec3 size = max - min;
  glm::vec3 sub_size = size / static_cast<float>(n);

  size_t iz = i / (n * n);
  size_t iy = (i % (n * n)) / n;
  size_t ix = i % n;

  glm::vec3 new_min = min + glm::vec3(static_cast<float>(ix) * sub_size.x,
                                      static_cast<float>(iy) * sub_size.y,
                                      static_cast<float>(iz) * sub_size.z);
  glm::vec3 new_max = new_min + sub_size;

  new_max = glm::min(new_max, max);

  return {new_min, new_max};
}

bool AABB::contains(const glm::vec3 &point) const {
  return point.x >= min.x && point.x <= max.x && point.y >= min.y &&
         point.y <= max.y && point.z >= min.z && point.z <= max.z;
}

float AABB::getRadius() const {
  return glm::length((max - min) * (sqrtf(2) / 2));
}

AABB AABB::translate(const glm::vec3 &direction) {
  return AABB{max + direction, min + direction};
}

std::ostream &operator<<(std::ostream &os, const AABB &aabb) {
  os << "AABB {\n"
     << "  min: (" << aabb.min.x << ", " << aabb.min.y << ", " << aabb.min.z
     << "),\n"
     << "  max: (" << aabb.max.x << ", " << aabb.max.y << ", " << aabb.max.z
     << ")\n"
     << "}";
  return os;
}
