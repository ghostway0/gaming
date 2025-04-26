#pragma once

#include "sunset/backend.h"
#include "sunset/ecs.h"

class DebugOverlay {
 public:
  DebugOverlay(Backend &backend);

  void update(ECS &ecs, std::vector<Command> &commands);

 private:
  Handle vertex_buffer_;
  Handle index_buffer_;
  Handle aabb_pipeline_;

  void initializePipeline(Backend &backend);
};

class RenderingSystem {
 public:
  RenderingSystem(Backend &backend);

  void update(ECS &ecs, std::vector<Command> &commands, bool debug = false);

 private:
  Handle pipeline_handle_;
  DebugOverlay debug_overlay_;

  void initializePipeline(Backend &backend);
};
