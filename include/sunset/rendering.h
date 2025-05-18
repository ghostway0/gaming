#pragma once

#include "sunset/backend.h"
#include "sunset/ecs.h"
#include "sunset/image.h"

class DebugOverlay {
 public:
  DebugOverlay(Backend &backend);

  void update(ECS &ecs, std::vector<Command> &commands);

 private:
  Pipeline text_pipeline_;
  Pipeline aabb_pipeline_;
  Font font_;

  void initializePipeline(Backend &backend);

  void drawText(const std::string &text, float x, float y,
                std::vector<Command> &commands);
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
