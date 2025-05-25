#include <ostream>

#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/projection.hpp>
#include <utility>
#include "sunset/geometry.h"
#include <absl/log/log.h>

#include "sunset/physics.h"

namespace {

void moveHierarchialAABB(ECS &ecs, Entity e, glm::vec3 direction) {
  PhysicsComponent *physics = ecs.getComponent<PhysicsComponent>(e);
  Transform *transform = ecs.getComponent<Transform>(e);

  physics->collider = physics->collider.translate(direction);

  for (Entity e : transform->children) {
    moveHierarchialAABB(ecs, e, direction);
  }
}

PhysicsMaterial combineMaterials(const PhysicsMaterial &a,
                                 const PhysicsMaterial &b) noexcept {
  return {a.friction * b.friction, a.restitution * b.restitution};
}

glm::vec3 calculateMTV(const AABB &a, const AABB &b) noexcept {
  glm::vec3 overlap_min = glm::max(a.min, b.min);
  glm::vec3 overlap_max = glm::min(a.max, b.max);

  glm::vec3 mtv{0.0f};
  float overlap_x = overlap_max.x - overlap_min.x;
  float overlap_y = overlap_max.y - overlap_min.y;
  float overlap_z = overlap_max.z - overlap_min.z;

  if (overlap_x < overlap_y && overlap_x < overlap_z) {
    mtv.x = a.min.x < b.min.x ? -overlap_x : overlap_x;
  } else if (overlap_y < overlap_z) {
    mtv.y = a.min.y < b.min.y ? -overlap_y : overlap_y;
  } else {
    mtv.z = a.min.z < b.min.z ? -overlap_z : overlap_z;
  }
  return mtv;
}

std::optional<glm::vec3> findIntersection(float ray_radius,
                                          glm::vec3 origin,
                                          glm::vec3 direction,
                                          AABB const &aabb) {
  AABB expanded = aabb;
  expanded.min -= glm::vec3(ray_radius);
  expanded.max += glm::vec3(ray_radius);

  glm::vec3 inv_dir = 1.0f / (direction + glm::epsilon<float>());
  glm::vec3 t1 = (expanded.min - origin) * inv_dir;
  glm::vec3 t2 = (expanded.max - origin) * inv_dir;

  glm::vec3 tmin = glm::min(t1, t2);
  glm::vec3 tmax = glm::max(t1, t2);

  float t_enter = std::max(std::max(tmin.x, tmin.y), tmin.z);
  float t_exit = std::min(std::min(tmax.x, tmax.y), tmax.z);

  if (t_exit < 0 || t_enter > t_exit) {
    return std::nullopt;
  }

  return origin + direction * t_enter;
}

glm::vec3 computeAABBCollisionNormal(AABB const &aabb, glm::vec3 origin,
                                     glm::vec3 direction) {
  glm::vec3 inv_dir = 1.0f / direction;
  glm::vec3 t1 = (aabb.min - origin) * inv_dir;
  glm::vec3 t2 = (aabb.max - origin) * inv_dir;

  glm::vec3 tmin = glm::min(t1, t2);
  glm::vec3 tmax = glm::max(t1, t2);

  float t_enter = std::max(std::max(tmin.x, tmin.y), tmin.z);
  float t_exit = std::min(std::min(tmax.x, tmax.y), tmax.z);

  if (t_exit < 0 || t_enter > t_exit) return {0, 0, 0};

  glm::vec3 normal(0.0f);
  if (t_enter == tmin.x) {
    normal.x = direction.x < 0 ? 1.0f : -1.0f;
  } else if (t_enter == tmin.y) {
    normal.y = direction.y < 0 ? 1.0f : -1.0f;
  } else {
    normal.z = direction.z < 0 ? 1.0f : -1.0f;
  }

  return normal;
}

} // namespace

PhysicsSystem &PhysicsSystem::instance() {
  static PhysicsSystem instance{};
  return instance;
}

bool PhysicsSystem::moveObject(ECS &ecs, Entity entity, glm::vec3 direction,
                               EventQueue &event_queue) {
  return moveObjectWithCollisions(ecs, entity, std::move(direction),
                                  1.0 / 60.0,
                                  event_queue); // the dt is weird
}

void PhysicsSystem::update(ECS &ecs, EventQueue &event_queue, float dt) {
  applyConstraintForces(ecs, dt);

  std::set<CollisionPair> old_collisions = new_collisions_;
  new_collisions_.clear();

  ecs.forEach(std::function([&](Entity entity, PhysicsComponent *physics,
                                Transform *transform) {
    if (physics->type == PhysicsComponent::Type::Static) {
      return;
    }

    physics->velocity += physics->acceleration;
    glm::vec3 velocity_scaled = physics->velocity * dt;

    if (physics->velocity == glm::vec3(0.0)) {
      return;
    }

    moveObjectWithCollisions(ecs, entity, velocity_scaled, dt, event_queue);
  }));

  generateColliderEvents(event_queue);
  collision_pairs_ = std::move(old_collisions);
}

std::optional<glm::vec3> PhysicsSystem::computeCollisionNormal(
    const PhysicsComponent &a_physics, const AABB &a_aabb,
    const PhysicsComponent &b_physics, const AABB &b_aabb) const noexcept {
  glm::vec3 normal;

  if (glm::length(a_physics.velocity) > glm::length(b_physics.velocity)) {
    normal = computeAABBCollisionNormal(b_aabb, a_aabb.getCenter(),
                                        a_physics.velocity);
  } else {
    normal = computeAABBCollisionNormal(a_aabb, b_aabb.getCenter(),
                                        b_physics.velocity);
  }

  if (glm::length(normal) < glm::epsilon<float>()) return std::nullopt;

  return glm::normalize(normal);
}

void PhysicsSystem::applyConstraintForces(ECS &ecs, float dt) noexcept {
  ecs.forEach(std::function([&](Entity entity, Constraint *constraint,
                                PhysicsComponent *physics,
                                Transform *transform) {
    Transform *b_transform = ecs.getComponent<Transform>(constraint->other);
    PhysicsComponent *b_physics =
        ecs.getComponent<PhysicsComponent>(constraint->other);

    glm::vec3 direction = b_transform->position - transform->position;
    float current_distance = glm::length(direction);
    float diff = current_distance - constraint->distance;

    if (std::abs(diff) < glm::epsilon<float>()) return;

    float correction = diff * 0.5f;
    glm::vec3 correction_vector = glm::normalize(direction) * correction;

    transform->position += correction_vector;
    b_transform->position -= correction_vector;

    glm::vec3 velocity_diff = b_physics->velocity - physics->velocity;
    float velocity_correction = glm::length(velocity_diff) * 0.5f;
    glm::vec3 velocity_correction_vector =
        glm::normalize(velocity_diff) * velocity_correction * dt;

    physics->velocity += velocity_correction_vector;
    b_physics->velocity -= velocity_correction_vector;

    glm::vec3 accel_diff = b_physics->acceleration - physics->acceleration;
    float accel_correction = glm::length(accel_diff) * 0.5f;
    glm::vec3 accel_correction_vector =
        glm::normalize(accel_diff) * accel_correction * dt;

    physics->acceleration += accel_correction_vector;
    b_physics->acceleration -= accel_correction_vector;
  }));
}

void PhysicsSystem::applyCollisionImpulse(PhysicsComponent *a_physics,
                                          PhysicsComponent *b_physics,
                                          glm::vec3 normal) {
  auto material =
      combineMaterials(a_physics->material, b_physics->material);

  if (a_physics->type == PhysicsComponent::Type::Regular &&
      b_physics->type == PhysicsComponent::Type::Regular) {
    float v1_normal = glm::dot(a_physics->velocity, normal);
    float v2_normal = glm::dot(b_physics->velocity, normal);

    float m1 = a_physics->mass;
    float m2 = b_physics->mass;
    float e = material.restitution;

    float new_v1_normal =
        (v1_normal * (m1 - e * m2) + v2_normal * (1 + e) * m2) / (m1 + m2);
    float new_v2_normal =
        (v2_normal * (m2 - e * m1) + v1_normal * (1 + e) * m1) / (m1 + m2);

    a_physics->velocity += normal * (new_v1_normal - v1_normal);
    b_physics->velocity += normal * (new_v2_normal - v2_normal);
    return;
  }

  if (a_physics->type == PhysicsComponent::Type::Regular) {
    glm::vec3 v1_normal = glm::proj(a_physics->velocity, normal);
    glm::vec3 v1_tangent = a_physics->velocity - v1_normal;
    a_physics->velocity =
        normal * glm::length(v1_normal) * material.restitution + v1_tangent;
  } else if (b_physics->type == PhysicsComponent::Type::Regular) {
    glm::vec3 v2_normal = glm::proj(b_physics->velocity, normal);
    glm::vec3 v2_tangent = b_physics->velocity - v2_normal;
    b_physics->velocity =
        normal * glm::length(v2_normal) * material.restitution + v2_tangent;
  }
}

void PhysicsSystem::resolveObjectOverlap(ECS &ecs, Entity a,
                                         Entity b) const {
  auto *a_physics = ecs.getComponent<PhysicsComponent>(a);
  auto *a_transform = ecs.getComponent<Transform>(a);
  auto *b_physics = ecs.getComponent<PhysicsComponent>(b);
  auto *b_transform = ecs.getComponent<Transform>(b);

  glm::vec3 mtv = calculateMTV(a_physics->collider, b_physics->collider);

  float scale = (a_physics->type == PhysicsComponent::Type::Regular &&
                 b_physics->type == PhysicsComponent::Type::Regular)
                    ? 0.5f
                    : 1.0f;
  glm::vec3 scaled_mtv = mtv * scale;

  if (a_physics->type == PhysicsComponent::Type::Regular) {
    a_transform->position += scaled_mtv;
    a_physics->collider = a_physics->collider.translate(scaled_mtv);
  }
  if (b_physics->type == PhysicsComponent::Type::Regular) {
    b_transform->position -= scaled_mtv;
    b_physics->collider = b_physics->collider.translate(-scaled_mtv);
  }
}

bool PhysicsSystem::moveObjectWithCollisions(ECS &ecs, Entity entity,
                                             glm::vec3 direction, float dt,
                                             EventQueue &event_queue) {
  bool found_collision = false;

  Transform *transform = ecs.getComponent<Transform>(entity);
  PhysicsComponent *physics = ecs.getComponent<PhysicsComponent>(entity);

  glm::vec3 old_velocity = physics->velocity;
  physics->velocity = direction;

  glm::vec3 moved = transform->position + direction * dt;
  AABB path_box = physics->collider.extendTo(moved);
  AABB aabb = physics->collider;

  glm::vec3 new_direction = direction;

  auto isCollider = [](PhysicsComponent::Type t) {
    return t == PhysicsComponent::Type::Collider;
  };

  auto isInfinite = [](PhysicsComponent::Type t) {
    return t == PhysicsComponent::Type::Infinite;
  };

  // TODO: use octree
  ecs.forEach(std::function(
      [&](Entity other, PhysicsComponent *other_physics, Transform *t) {
        if (new_direction == glm::vec3(0.0)) {
          return;
        }

        if (entity == other) return;

        // if (auto intersection = findIntersection(
        //         aabb.getRadius(), aabb.getCenter(), direction,
        //         other_aabb);
        //     intersection.has_value()) {
        //   transform->bounding_box = transform->bounding_box.translate(
        //       *intersection - transform->position);
        //   new_direction -= *intersection - transform->position;
        //   transform->position = *intersection;
        // }

        AABB other_aabb = other_physics->collider;
        if (!path_box.intersects(other_aabb)) {
          return;
        }

        std::optional<glm::vec3> normal = computeCollisionNormal(
            *physics, aabb, *other_physics, other_aabb);

        if (!normal) {
          resolveObjectOverlap(ecs, entity, other);
          return;
        }

        bool is_collider =
            isCollider(physics->type) || isCollider(other_physics->type);

        if (!is_collider && !(isInfinite(physics->type) &&
                              isInfinite(other_physics->type))) {
          applyCollisionImpulse(physics, other_physics, *normal);
        }

        if (is_collider) {
          Entity collider = isCollider(physics->type)
                                ? physics->collision_source
                                : other_physics->collision_source;
          Entity collided = (collider == entity)
                                ? other_physics->collision_source
                                : physics->collision_source;

          new_collisions_.insert({collided, collider});
        } else if (glm::length(physics->velocity) > 0.001 ||
                   glm::length(other_physics->velocity) > 0.001) {
          event_queue.send(Collision{
              physics->collision_source, other_physics->collision_source,
              physics->velocity, other_physics->velocity});
        }

        if (other_physics->type == PhysicsComponent::Type::Infinite) {
          glm::vec3 normal_direction = glm::proj(direction, *normal);
          new_direction -= normal_direction;
        }

        if (physics->collider.intersects(other_physics->collider)) {
          resolveObjectOverlap(ecs, entity, other);
          new_direction = glm::vec3(0.0);
        }

        found_collision = true;
      }));

  physics->velocity = old_velocity;

  transform->position += new_direction;
  moveHierarchialAABB(ecs, entity, new_direction);

  return found_collision;
}

void PhysicsSystem::generateColliderEvents(EventQueue &event_queue) {
  std::vector<CollisionPair> entered;
  std::vector<CollisionPair> exited;

  std::set_difference(new_collisions_.begin(), new_collisions_.end(),
                      collision_pairs_.begin(), collision_pairs_.end(),
                      std::back_inserter(entered));
  std::set_difference(collision_pairs_.begin(), collision_pairs_.end(),
                      new_collisions_.begin(), new_collisions_.end(),
                      std::back_inserter(exited));

  for (const auto &pair : entered) {
    event_queue.send(EnterCollider{pair.entity_a, pair.entity_b});
  }

  for (const auto &pair : exited) {
    event_queue.send(ExitCollider{pair.entity_a, pair.entity_b});
  }
}
