#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "sunset/camera.h"

glm::mat4 calculateViewMatrix(Camera *camera, Transform *transform) {
  glm::vec3 forward = transform->rotation * glm::vec3(0, 0, -1);
  glm::vec3 up = transform->rotation * glm::vec3(0, 1, 0);
  return glm::lookAt(transform->position, transform->position + forward,
                     up);
}

glm::mat4 calculateProjectionMatrix(Camera *camera, Transform *transform) {
  return glm::perspective(camera->fov, camera->aspect, 0.1f, 100.0f);
}

bool isSphereInFrustum(const glm::vec3 &center, float radius,
                       glm::mat4 view_projection) {
  glm::vec4 sphere_center(center, 1.0f);
  glm::vec4 clip = view_projection * sphere_center;

  return within(clip.x, -clip.w - radius, clip.w + radius) ||
         within(clip.y, -clip.w - radius, clip.w + radius) ||
         within(clip.z, -clip.w - radius, clip.w + radius);
}

bool isBoxInFrustum(const AABB &aabb, glm::mat4 view_projection) {
  glm::vec3 center;
  float radius = aabb.getRadius();
  aabb.getCenter();
  return isSphereInFrustum(center, radius, view_projection);
}
