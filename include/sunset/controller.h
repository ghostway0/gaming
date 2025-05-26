#pragma once

#include "sunset/ecs.h"
#include "sunset/event_queue.h"

struct Player {
  float speed;
  float sensitivity;
  bool sprinting;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"Player"};
    return tree;
  }

  static absl::StatusOr<Player> deserialize(
      PropertyTree const & /* tree */) {
    // TODO:
    return absl::InternalError("TODO");
  }
};

class FreeController {
 public:
  FreeController(ECS &ecs, EventQueue &event_queue);

  void update(ECS &ecs);
};

// template <typename C>
class PlayerController {
 public:
  PlayerController(ECS &ecs, EventQueue &event_queue);

  void update(ECS &ecs);
};
