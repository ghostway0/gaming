#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "sunset/geometry.h"

AABB::AABB(glm::vec3 max, glm::vec3 min) : max_{max}, min_{min} {}

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity) {
  glm::mat4 model_matrix = glm::mat4(1.0f);

  Entity current = entity;

  while (true) {
    const Transform *transform = ecs.getComponent<Transform>(current);

    glm::mat4 local = glm::mat4(1.0f);

    float angle = glm::length(transform->rotation);
    if (angle > 1e-6f) {
      glm::vec3 axis = glm::normalize(transform->rotation);
      local = glm::rotate(local, angle, axis);
    }

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
  std::span<const uint8_t> vertex_data{
      reinterpret_cast<const uint8_t *>(mesh.vertices.data()),
      mesh.vertices.size() * sizeof(Vertex)};

  std::span<const uint8_t> index_data{
      reinterpret_cast<const uint8_t *>(mesh.indices.data()),
      mesh.indices.size() * sizeof(uint32_t)};

  return MeshRenderable{
      .vertex_buffer = backend.upload(vertex_data),
      .vertex_count = mesh.vertices.size(),
      .index_buffer = backend.upload(index_data),
      .index_count = mesh.indices.size(),
      .normal = mesh.normal,
  };
}
