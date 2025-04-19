#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "sunset/camera.h"

Camera::Camera(const State& state, const Options& options) {
  init(state, options);
}

void Camera::init(const Camera::State& state,
                      const Camera::Options& options) {
  position_ = state.position;
  up_ = state.up;
  yaw_ = state.yaw;
  pitch_ = state.pitch;

  this->setRotation(yaw_, pitch_);

  fov_ = options.fov;
  sensitivity_ = options.sensitivity;
  speed_ = options.speed;
  aspect_ratio_ = options.aspect_ratio;

  calculateProjectionMatrix();
}

void Camera::rotateAbsolute(float x_angle, float y_angle) {
  pitch_ = std::clamp(pitch_ + y_angle, -glm::half_pi<float>(),
                      glm::half_pi<float>());
  yaw_ = std::fmod(yaw_ + x_angle, glm::two_pi<float>());

  glm::quat yaw_quat = glm::angleAxis(x_angle, world_up_);
  glm::quat pitch_quat = glm::angleAxis(y_angle, right_);
  glm::quat rotation = yaw_quat * pitch_quat;

  direction_ = glm::normalize(rotation * direction_);
  up_ = glm::normalize(rotation * up_);
  right_ = glm::normalize(glm::cross(direction_, up_));

  calculateViewMatrix();
}

void Camera::setRotation(float x_angle, float y_angle) {
  yaw_ = std::fmod(x_angle, glm::two_pi<float>());
  pitch_ =
      std::clamp(y_angle, -glm::half_pi<float>(), glm::half_pi<float>());

  direction_ = glm::normalize(
      glm::vec3{std::sin(pitch_), -std::sin(yaw_) * std::cos(pitch_),
                -std::cos(yaw_) * std::cos(pitch_)});

  world_up_ = glm::vec3(0.0f, 1.0f, 0.0f);
  up_ = world_up_;

  right_ = glm::normalize(glm::cross(up_, direction_));
  up_ = glm::normalize(glm::cross(direction_, right_));

  calculateViewMatrix();
}

void Camera::rotateScaled(float x_angle, float y_angle) {
  rotateAbsolute(x_angle * sensitivity_, y_angle * sensitivity_);
}

void Camera::moveAbsolute(const glm::vec3& direction) {
  position_ += direction;
  calculateViewMatrix();
}

void Camera::moveScaled(glm::vec3 direction, float dt) {
  position_ += direction * speed_ * dt;
  calculateViewMatrix();
}

void Camera::recalculateVectors() {
  setRotation(yaw_, pitch_);
  calculateProjectionMatrix();
}

void Camera::setAspectRatio(float ratio) {
  aspect_ratio_ = ratio;
  calculateProjectionMatrix();
}

// bool isSphereInFrustum(const glm::vec3& center, float radius) const {
//   glm::vec4 sphere_center(center, 1.0f);
//   glm::mat4 view_projection = projection_matrix_ * view_matrix_;
//   glm::vec4 clip = view_projection * sphere_center;
//
//   return within(clip.x, -clip.w - radius, clip.w + radius) ||
//          within(clip.y, -clip.w - radius, clip.w + radius) ||
//          within(clip.z, -clip.w - radius, clip.w + radius);
// }

bool Camera::isPointInFrustum(const glm::vec3& point) const {
  glm::vec4 p(point, 1.0f);
  glm::mat4 view_projection = projection_matrix_ * view_matrix_;
  glm::vec4 clip = view_projection * p;

  return std::abs(clip.x) < clip.w && std::abs(clip.y) < clip.w &&
         std::abs(clip.z) < clip.w;
}

// bool isBoxInFrustum(const AABB& aabb) const {
//   glm::vec3 center;
//   float radius = aabb_get_radius(&aabb);
//   aabb_get_center(&aabb, center);
//   return isSphereInFrustum(center, radius);
// }

// bool isCrosshairOver(const AABB& aabb) const {
//   return ray_intersects_aabb(position_, direction_, &aabb, nullptr);
// }

void Camera::vecToWorld(glm::vec3& direction) const {
  glm::quat yaw_rot = glm::angleAxis(yaw_, world_up_);
  glm::quat pitch_rot = glm::angleAxis(pitch_, right_);
  glm::quat full_rot = yaw_rot * pitch_rot;
  direction = full_rot * direction;
}

void Camera::calculateViewMatrix() {
  view_matrix_ = glm::lookAt(position_, position_ + direction_, up_);
}

void Camera::calculateProjectionMatrix() {
  projection_matrix_ = glm::perspective(fov_, aspect_ratio_, 0.1f, 100.0f);
}
