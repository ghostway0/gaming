#pragma once

#include "sunset/ecs.h"
#include "sunset/event_queue.h"

struct Player {
  float speed;
  float sensitivity;
  bool sprinting;

  void serialize(std::ostream &os) const {}

  static Player deserialize(std::istream &is) { return {}; }
};

class PlayerController {
 public:
  PlayerController(ECS &ecs, EventQueue &event_queue);

  void update(ECS &ecs);

 private:
};
