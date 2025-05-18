#pragma once

#include "sunset/backend.h"
#include "sunset/ecs.h"
#include "sunset/image.h"

class DebugOverlay {
 public:
  DebugOverlay(Backend &backend);

  void update(ECS &ecs, std::vector<Command> &commands);

 private:
  Handle vertex_buffer_;
  Handle text_vertex_buffer_;
  Handle text_index_buffer_;
  Handle font_texture_;
  Handle index_buffer_;
  Handle text_pipeline_;
  Handle aabb_pipeline_;
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
