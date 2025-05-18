#pragma once

#include <cassert>
#include <vector>
#include <span>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <absl/log/log.h>
#include <glm/glm.hpp>

#include "sunset/backend.h"

struct CompiledPipeline {
  GLuint program_handle;
  PipelineLayout layout;
  GLuint vao;
};

class OpenGLBackend : public Backend {
 public:
  void interpret(std::span<const Command> commands) override;

  Handle compilePipeline(Pipeline pipeline) override;

  Handle upload(std::span<const uint8_t> buffer) override;

  Handle allocDynamic(size_t size) override;
  
  Handle uploadTexture(const Image &image) override;

 private:
  std::vector<CompiledPipeline> pipelines_;
  Handle current_;

  GLuint compileShader(Shader const &shader);

  std::optional<CompiledPipeline> getCurrentPipeline();

  void handleCommand(const BindBuffer &cmd);

  void handleCommand(const BindVertexBuffer &cmd);

  void handleCommand(const BindIndexBuffer &cmd);

  void handleCommand(const BindTexture &cmd);

  void handleCommand(const UpdateBuffer &cmd);

  void handleCommand(const Use &cmd);

  void handleCommand(const SetUniform &cmd);

  void handleCommand(const Draw &cmd);

  void handleCommand(const SetViewport &cmd);

  void handleCommand(const DrawIndexed &cmd);
};
