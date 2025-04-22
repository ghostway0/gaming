#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

class Camera {
 public:
  struct State {
    glm::vec3 position;
    glm::vec3 up;
    float yaw;
    float pitch;
  };

  struct Options {
    float fov;
    float sensitivity;
    float speed;
    float aspect_ratio;
  };

  Camera(const State& state, const Options& options);

  void rotateAbsolute(float x_angle, float y_angle);

  void setRotation(float x_angle, float y_angle);

  void rotateScaled(float x_angle, float y_angle);

  void moveAbsolute(const glm::vec3& direction);

  void moveScaled(glm::vec3 direction, float dt);

  void recalculateVectors();

  void setAspectRatio(float ratio);

  // bool isSphereInFrustum(const glm::vec3& center, float radius) const {
  //   glm::vec4 sphere_center(center, 1.0f);
  //   glm::mat4 view_projection = projection_matrix_ * view_matrix_;
  //   glm::vec4 clip = view_projection * sphere_center;
  //
  //   return within(clip.x, -clip.w - radius, clip.w + radius) ||
  //          within(clip.y, -clip.w - radius, clip.w + radius) ||
  //          within(clip.z, -clip.w - radius, clip.w + radius);
  // }

  bool isPointInFrustum(const glm::vec3& point) const;

  // bool isBoxInFrustum(const AABB& aabb) const {
  //   glm::vec3 center;
  //   float radius = aabb_get_radius(&aabb);
  //   aabb_get_center(&aabb, center);
  //   return isSphereInFrustum(center, radius);
  // }

  // bool isCrosshairOver(const AABB& aabb) const {
  //   return ray_intersects_aabb(position_, direction_, &aabb, nullptr);
  // }

  void vecToWorld(glm::vec3& direction) const;

  const glm::mat4& viewMatrix() const { return view_matrix_; }
  const glm::mat4& projectionMatrix() const { return projection_matrix_; }
  const glm::vec3& position() const { return position_; }
  const glm::vec3& direction() const { return direction_; }

  void serialize(std::ostream& os) const {
    os << position_.x << " " << position_.y << " " << position_.z << "\n";
    os << direction_.x << " " << direction_.y << " " << direction_.z
       << "\n";
    os << up_.x << " " << up_.y << " " << up_.z << "\n";
    os << right_.x << " " << right_.y << " " << right_.z << "\n";
    os << world_up_.x << " " << world_up_.y << " " << world_up_.z << "\n";

    os << yaw_ << " " << pitch_ << " " << fov_ << " " << sensitivity_ << " "
       << speed_ << " " << aspect_ratio_ << "\n";

    for (int i = 0; i < 4; ++i)
      os << projection_matrix_[i][0] << " " << projection_matrix_[i][1]
         << " " << projection_matrix_[i][2] << " "
         << projection_matrix_[i][3] << "\n";

    for (int i = 0; i < 4; ++i)
      os << view_matrix_[i][0] << " " << view_matrix_[i][1] << " "
         << view_matrix_[i][2] << " " << view_matrix_[i][3] << "\n";
  }

  static Camera deserialize(std::istream& is) {
    Camera camera{{}, {}};

    is >> camera.position_.x >> camera.position_.y >> camera.position_.z;
    is >> camera.direction_.x >> camera.direction_.y >> camera.direction_.z;
    is >> camera.up_.x >> camera.up_.y >> camera.up_.z;
    is >> camera.right_.x >> camera.right_.y >> camera.right_.z;
    is >> camera.world_up_.x >> camera.world_up_.y >> camera.world_up_.z;

    is >> camera.yaw_ >> camera.pitch_ >> camera.fov_ >>
        camera.sensitivity_ >> camera.speed_ >> camera.aspect_ratio_;

    for (int i = 0; i < 4; ++i)
      is >> camera.projection_matrix_[i][0] >>
          camera.projection_matrix_[i][1] >>
          camera.projection_matrix_[i][2] >>
          camera.projection_matrix_[i][3];

    for (int i = 0; i < 4; ++i)
      is >> camera.view_matrix_[i][0] >> camera.view_matrix_[i][1] >>
          camera.view_matrix_[i][2] >> camera.view_matrix_[i][3];

    return camera;
  }

 private:
  void init(const State& state, const Options& options);

  void calculateViewMatrix();

  void calculateProjectionMatrix();

  glm::vec3 position_;
  glm::vec3 direction_;
  glm::vec3 up_;
  glm::vec3 right_;
  glm::vec3 world_up_;
  float yaw_ = 0.0f;
  float pitch_ = 0.0f;
  float fov_ = glm::radians(60.0f);
  float sensitivity_ = 0.1f;
  float speed_ = 5.0f;
  float aspect_ratio_ = 16.0f / 9.0f;

  glm::mat4 view_matrix_ = glm::mat4(1.0f);
  glm::mat4 projection_matrix_ = glm::mat4(1.0f);
};
