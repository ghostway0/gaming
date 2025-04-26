#pragma once

#include <set>
#include <algorithm>
#include <optional>

#include "sunset/event_queue.h"
#include "sunset/ecs.h"
#include "sunset/geometry.h"

struct PhysicsComponent {
  enum class Type { Regular, Infinite, Collider };

  glm::vec3 velocity{0.0f};
  glm::vec3 acceleration{0.0f};
  float mass{1.0f};
  Type type{Type::Regular};
  struct Material {
    float friction{0.5f};
    float restitution{0.0f};
  } material;

  void serialize(std::ostream &os) const {}

  static PhysicsComponent deserialize(std::istream &is) { return {}; }
};

struct EnterCollider {
  Entity entity;
  Entity collider;
};

struct ExitCollider {
  Entity entity;
  Entity collider;
};

struct Collision {
  Entity entity_a;
  Entity entity_b;
  glm::vec3 velocity_a;
  glm::vec3 velocity_b;
};

struct Constraint {
  Entity other;
  float distance;
};

struct CollisionPair {
  Entity entity_a;
  Entity entity_b;

  bool operator==(const CollisionPair &other) const noexcept {
    return (entity_a == other.entity_a && entity_b == other.entity_b) ||
           (entity_a == other.entity_b && entity_b == other.entity_a);
  }

  friend bool operator<(const CollisionPair &a,
                        const CollisionPair &b) noexcept {
    auto a_min = std::min(a.entity_a, a.entity_b);
    auto a_max = std::max(a.entity_a, a.entity_b);
    auto b_min = std::min(b.entity_a, b.entity_b);
    auto b_max = std::max(b.entity_a, b.entity_b);
    return std::tie(a_min, a_max) < std::tie(b_min, b_max);
  }
};

struct CollisionData {
  glm::vec3 normal;
  glm::vec3 mtv;
  bool is_collider{false};
};

class PhysicsSystem {
  static constexpr float kVelocityEpsilon = 0.0001f;

 public:
  static PhysicsSystem &instance();

  bool moveObject(ECS &ecs, Entity entity, glm::vec3 direction,
                  EventQueue &event_queue);

  void update(ECS &ecs, EventQueue &event_queue, float dt);

 private:
  std::set<CollisionPair> collision_pairs_;
  std::set<CollisionPair> new_collisions_;

  bool moveObjectWithCollisions(ECS &ecs, Entity entity,
                                glm::vec3 direction,
                                EventQueue &event_queue);

  std::optional<glm::vec3> computeCollisionNormal(
      const PhysicsComponent &a_physics, const AABB &a_aabb,
      const PhysicsComponent &b_physics, const AABB &b_aabb) const noexcept;

  void applyConstraintForces(ECS &ecs, float dt) noexcept;

  void applyCollisionImpulse(PhysicsComponent *a_physics,
                             PhysicsComponent *b_physics,
                             const CollisionData &collision);

  void resolveObjectOverlap(ECS &ecs, Entity a, Entity b,
                            const glm::vec3 &mtv);

  void generateColliderEvents(EventQueue &event_queue);
};
