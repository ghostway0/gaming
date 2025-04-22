#pragma once

#include "sunset/backend.h"
#include "sunset/camera.h"
#include "sunset/ecs.h"

class RenderingSystem {
 public:
  RenderingSystem(Backend &backend);

  void update(ECS &ecs, Camera const &camera, std::vector<Command> &commands);

 private:
  Handle pipeline_handle_ = 0;

  void initializePipeline(Backend &backend);
};
