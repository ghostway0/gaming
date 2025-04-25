#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ostream>
#include <glm/gtx/quaternion.hpp>

#include "sunset/geometry.h"

struct Camera {
  Rect viewport;
  float fov;
  float aspect;

  void serialize(std::ostream &os) const {}

  static Camera deserialize(std::istream &is) { return {}; }
};

glm::mat4 calculateViewMatrix(Camera *camera, Transform *transform);

glm::mat4 calculateProjectionMatrix(Camera *camera, Transform *transform);

inline bool within(float value, float min, float max) {
  return value >= min && value <= max;
}

bool isSphereInFrustum(const glm::vec3 &center, float radius,
                       glm::mat4 view_projection);

bool isBoxInFrustum(const AABB &aabb, glm::mat4 view_projection);
