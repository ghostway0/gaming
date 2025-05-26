#pragma once

#include <set>
#include <algorithm>
#include <optional>

#include "sunset/event_queue.h"
#include "sunset/ecs.h"
#include "sunset/geometry.h"
#include "sunset/property_tree.h"

struct PhysicsMaterial {
  float friction{0.5f};
  float restitution{1.0f};
};

template <>
struct TypeDeserializer<PhysicsMaterial> {
  static std::vector<FieldDescriptor<PhysicsMaterial>> getFields() {
    return {
        makeSetter("Friction", &PhysicsMaterial::friction, true),
        makeSetter("Restitution", &PhysicsMaterial::restitution, true),
    };
  }
};

struct PhysicsComponent {
  enum class Type { Regular, Infinite, Collider, Static };

  glm::vec3 velocity{0.0f};
  glm::vec3 acceleration{0.0f};
  float mass{1.0f};
  Type type{Type::Regular};
  PhysicsMaterial material;
  AABB collider;
  Entity collision_source;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"PhysicsComponent"};
    return tree;
  }

  static absl::StatusOr<PhysicsComponent> deserialize(
      PropertyTree const &tree) {
    return deserializeTree<PhysicsComponent>(tree);
  }
};

template <>
inline absl::StatusOr<PhysicsComponent::Type> deserializeTree(
    const PropertyTree &tree) {
  if (tree.properties.size() < 1) {
    return absl::InvalidArgumentError("Invalid physics object type");
  }
  return static_cast<PhysicsComponent::Type>(
      TRY(extractProperty<int16_t>(tree.properties[0])));
}

template <>
struct TypeDeserializer<PhysicsComponent> {
  static std::vector<FieldDescriptor<PhysicsComponent>> getFields() {
    return {
        makeSetter("Velocity", &PhysicsComponent::velocity, true),
        makeSetter("Acceleration", &PhysicsComponent::acceleration, true),
        makeSetter("Mass", &PhysicsComponent::mass, true),
        makeSetter("Type", &PhysicsComponent::type, true),
        makeSetter("Material", &PhysicsComponent::material),
        makeSetter("Collider", &PhysicsComponent::collider),
        makeSetter("CollisionSource", &PhysicsComponent::collision_source),

    };
  }
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
                                glm::vec3 direction, float dt,
                                EventQueue &event_queue);

  std::optional<glm::vec3> computeCollisionNormal(
      const PhysicsComponent &a_physics, const AABB &a_aabb,
      const PhysicsComponent &b_physics, const AABB &b_aabb) const noexcept;

  void applyConstraintForces(ECS &ecs, float dt) noexcept;

  void applyCollisionImpulse(PhysicsComponent *a_physics,
                             PhysicsComponent *b_physics, glm::vec3 normal);

  void resolveObjectOverlap(ECS &ecs, Entity a, Entity b) const;

  void generateColliderEvents(EventQueue &event_queue);
};
