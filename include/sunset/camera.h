#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <absl/status/status.h>

#include "sunset/geometry.h"

struct Camera {
  Rect viewport;
  float fov;
  float aspect;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"DamageComponent"};
    return tree;
  }

  static absl::StatusOr<Camera> deserialize(
      PropertyTree const & /* tree */) {
    return absl::InternalError("TODO");
  }
};

glm::mat4 calculateViewMatrix(Camera *camera, Transform *transform);

glm::mat4 calculateProjectionMatrix(Camera *camera, Transform *transform);

inline bool within(float value, float min, float max) {
  return value >= min && value <= max;
}

bool isSphereInFrustum(const glm::vec3 &center, float radius,
                       glm::mat4 view_projection);

bool isBoxInFrustum(const AABB &aabb, glm::mat4 view_projection);
