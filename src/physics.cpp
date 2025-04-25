#include <glm/ext/scalar_constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/projection.hpp>

#include "sunset/physics.h"

namespace {

bool isZeroVector(const glm::vec3 &vector, float epsilon) noexcept {
  return glm::length(vector) < epsilon;
}

PhysicsComponent::Material combineMaterials(
    const PhysicsComponent::Material &a,
    const PhysicsComponent::Material &b) noexcept {
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

glm::vec3 computeAABBCollisionNormal(const AABB &aabb,
                                     const glm::vec3 &direction) noexcept {
  glm::vec3 center = aabb.getCenter();
  glm::vec3 inv_dir = 1.0f / (direction + glm::vec3(1e-6f));

  float tmin = (aabb.min.x - center.x) * inv_dir.x;
  float tmax = (aabb.max.x - center.x) * inv_dir.x;
  size_t axis = 0;

  float tmin_temp = (aabb.min.y - center.y) * inv_dir.y;
  float tmax_temp = (aabb.max.y - center.y) * inv_dir.y;
  if (tmin_temp > tmin) {
    tmin = tmin_temp;
    axis = 1;
  }
  if (tmax_temp < tmax) {
    tmax = tmax_temp;
    axis = 1;
  }

  tmin_temp = (aabb.min.z - center.z) * inv_dir.z;
  tmax_temp = (aabb.max.z - center.z) * inv_dir.z;
  if (tmin_temp > tmin) {
    tmin = tmin_temp;
    axis = 2;
  }
  if (tmax_temp < tmax) {
    tmax = tmax_temp;
    axis = 2;
  }

  glm::vec3 normal_out = glm::vec3(0.0f);
  normal_out[axis] = tmin < tmax ? 1.0f : -1.0f;
  return normal_out;
}

} // namespace

PhysicsSystem &PhysicsSystem::instance() {
  static PhysicsSystem instance{};
  return instance;
}

bool PhysicsSystem::moveObject(ECS &ecs, Entity entity, glm::vec3 direction,
                               EventQueue &event_queue) {
  return moveObjectWithCollisions(ecs, entity, std::move(direction),
                                  event_queue, nullptr);
}

void PhysicsSystem::update(ECS &ecs, EventQueue &event_queue, float dt) {
  applyConstraintForces(ecs, dt);

  std::set<CollisionPair> new_collisions;

  ecs.forEach(std::function(
      [&](Entity entity, PhysicsComponent *physics, Transform *transform) {
        physics->velocity += physics->acceleration;
        glm::vec3 velocity_scaled = physics->velocity * dt;

        moveObjectWithCollisions(ecs, entity, velocity_scaled, event_queue,
                                 &new_collisions);
      }));

  generateColliderEvents(event_queue, new_collisions);
  collision_pairs_ = std::move(new_collisions);
}

std::optional<glm::vec3> PhysicsSystem::computeCollisionNormal(
    const PhysicsComponent &a_physics, const AABB &a_aabb,
    const PhysicsComponent &b_physics, const AABB &b_aabb) const noexcept {
  glm::vec3 normal;

  if (glm::length(a_physics.velocity) > glm::length(b_physics.velocity)) {
    normal = computeAABBCollisionNormal(b_aabb, a_physics.velocity);
  } else {
    normal = computeAABBCollisionNormal(a_aabb, b_physics.velocity);
  }

  if (glm::length(normal) < 1e-6f) return std::nullopt;

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

    if (std::abs(diff) < 1e-6f) return;

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
                                          const CollisionData &collision) {
  auto material =
      combineMaterials(a_physics->material, b_physics->material);
  const auto &normal = collision.normal;

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
        normal * (-glm::length(v1_normal) * material.restitution) +
        v1_tangent;
  } else if (b_physics->type == PhysicsComponent::Type::Regular) {
    glm::vec3 v2_normal = glm::proj(b_physics->velocity, normal);
    glm::vec3 v2_tangent = b_physics->velocity - v2_normal;
    b_physics->velocity =
        normal * (-glm::length(v2_normal) * material.restitution) +
        v2_tangent;
  }
}

void PhysicsSystem::resolveObjectOverlap(ECS &ecs, Entity a, Entity b,
                                         const glm::vec3 &mtv) {
  auto *a_physics = ecs.getComponent<PhysicsComponent>(a);
  auto *a_transform = ecs.getComponent<Transform>(a);
  auto *b_physics = ecs.getComponent<PhysicsComponent>(b);
  auto *b_transform = ecs.getComponent<Transform>(b);

  float scale = (a_physics->type == PhysicsComponent::Type::Regular &&
                 b_physics->type == PhysicsComponent::Type::Regular)
                    ? 0.5f
                    : 1.0f;
  glm::vec3 scaled_mtv = mtv * scale;

  if (a_physics->type == PhysicsComponent::Type::Regular) {
    a_transform->position += scaled_mtv;
  }
  if (b_physics->type == PhysicsComponent::Type::Regular) {
    b_transform->position -= scaled_mtv;
  }
}

bool PhysicsSystem::moveObjectWithCollisions(
    ECS &ecs, Entity entity, glm::vec3 direction, EventQueue &event_queue,
    std::set<CollisionPair> *new_collisions) {
  bool found_collision = false;

  Transform *transform = ecs.getComponent<Transform>(entity);
  PhysicsComponent *physics = ecs.getComponent<PhysicsComponent>(entity);

  glm::vec3 moved = transform->position + direction;
  AABB path_box = transform->bounding_box.extendTo(moved);
  AABB aabb = transform->bounding_box;

  glm::vec3 new_direction = direction;

  auto isCollider = [](PhysicsComponent::Type t) {
    return t == PhysicsComponent::Type::Collider;
  };

  auto isInfinite = [](PhysicsComponent::Type t) {
    return t == PhysicsComponent::Type::Infinite;
  };

  ecs.forEach(std::function([&](Entity other, Transform *t) {
    if (entity == other) return;

    AABB other_aabb = t->bounding_box;
    if (!path_box.intersects(other_aabb)) return;

    auto *other_physics = ecs.getComponent<PhysicsComponent>(other);
    if (!other_physics) return;

    auto normal =
        computeCollisionNormal(*physics, aabb, *other_physics, other_aabb);
    if (!normal) return;

    glm::vec3 mtv = calculateMTV(aabb, other_aabb);
    CollisionData collision{
        *normal, mtv,
        isCollider(physics->type) || isCollider(other_physics->type)};

    glm::vec3 normal_direction = glm::proj(direction, collision.normal);
    new_direction -= normal_direction;

    if (!isZeroVector(physics->velocity, kVelocityEpsilon) ||
        !isZeroVector(other_physics->velocity, kVelocityEpsilon)) {
      event_queue.send(Collision{entity, other, physics->velocity,
                                 other_physics->velocity});
    }

    if (!collision.is_collider &&
        !(isInfinite(physics->type) && isInfinite(other_physics->type))) {
      applyCollisionImpulse(physics, other_physics, collision);
    }

    if (new_collisions && collision.is_collider) {
      Entity collider = isCollider(physics->type) ? entity : other;
      Entity collided = (collider == entity) ? other : entity;
      new_collisions->insert({collider, collided});
    }

    if (aabb.intersects(other_aabb)) {
      resolveObjectOverlap(ecs, entity, other, mtv);
    }

    found_collision = true;
  }));

  transform->position += new_direction;
  return found_collision;
}

void PhysicsSystem::generateColliderEvents(
    EventQueue &event_queue,
    const std::set<CollisionPair> &new_collisions) {
  std::vector<CollisionPair> entered;
  std::vector<CollisionPair> exited;

  std::set_difference(new_collisions.begin(), new_collisions.end(),
                      collision_pairs_.begin(), collision_pairs_.end(),
                      std::back_inserter(entered));
  std::set_difference(collision_pairs_.begin(), collision_pairs_.end(),
                      new_collisions.begin(), new_collisions.end(),
                      std::back_inserter(exited));

  for (const auto &pair : entered) {
    event_queue.send(EnterCollider{pair.entity_a, pair.entity_b});
  }

  for (const auto &pair : exited) {
    event_queue.send(ExitCollider{pair.entity_a, pair.entity_b});
  }
}
