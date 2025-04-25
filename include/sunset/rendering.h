#pragma once

#include "sunset/backend.h"
#include "sunset/ecs.h"

class RenderingSystem {
 public:
  RenderingSystem(Backend &backend);

  void update(ECS &ecs, std::vector<Command> &commands);

 private:
  Handle pipeline_handle_;

  void initializePipeline(Backend &backend);
};
