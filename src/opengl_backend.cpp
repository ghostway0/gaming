#include <cassert>
#include <iostream>
#include <vector>
#include <span>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <absl/log/log.h>
#include <glm/glm.hpp>

#include "sunset/backend.h"

namespace {

GLenum primitiveToSys(PrimitiveTopology primitive) {
  switch (primitive) {
    case PrimitiveTopology::Triangles:
      return GL_TRIANGLES;
    case PrimitiveTopology::Lines:
      return GL_LINES;
    case PrimitiveTopology::Points:
      return GL_POINTS;
  }
}

} // namespace

class OpenGLBackend : public Backend {
 public:
  void interpret(std::span<const Command> commands) override {
    for (const auto &command : commands) {
      std::visit([this](const auto &cmd) { this->handleCommand(cmd); },
                 command);
    }
    assert(glGetError() == GL_NO_ERROR);
  }

  Handle compilePipeline(Pipeline pipeline) override {
    GLuint program = glCreateProgram();

    std::vector<GLuint> shader_handles;
    for (const Shader &shader : pipeline.shaders) {
      GLuint handle = compileShader(shader);
      glAttachShader(program, handle);
      shader_handles.push_back(handle);
    }

    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
      GLint logLength;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
      std::vector<char> log(logLength);
      glGetProgramInfoLog(program, logLength, nullptr, log.data());
      std::cerr << "Shader Program Linking Failed: " << log.data()
                << std::endl;
    }

    for (GLuint handle : shader_handles) {
      glDeleteShader(handle);
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindVertexArray(0);

    pipelines_.emplace_back(CompiledPipeline{
        .program_handle = program, .layout = pipeline.layout, .vao = vao});

    return pipelines_.size();
  }

  Handle upload(std::span<const uint8_t> buffer) override {
    GLuint buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return buffer_id;
  }

  Handle allocDynamic(size_t size) override {
    GLuint buffer_id;
    glGenBuffers(1, &buffer_id);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    return buffer_id;
  }

 private:
  struct CompiledPipeline {
    GLuint program_handle;
    PipelineLayout layout;
    GLuint vao;
  };

  std::vector<CompiledPipeline> pipelines_;
  Handle current_;

  GLuint compileShader(Shader const &shader) {
    GLuint handle = glCreateShader((shader.type == ShaderType::Vertex)
                                       ? GL_VERTEX_SHADER
                                       : GL_FRAGMENT_SHADER);
    const char *shader_source = shader.source.c_str();
    glShaderSource(handle, 1, &shader_source, nullptr);
    glCompileShader(handle);

    GLint success;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
    if (!success) {
      GLint logLength;
      glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLength);
      std::vector<char> log(logLength);
      glGetShaderInfoLog(handle, logLength, nullptr, log.data());
      LOG(WARNING) << "Shader Compilation Failed: " << log.data();
    }
    return handle;
  }

  void handleCommand(const BindBuffer &cmd) {
    glBindBuffer(GL_ARRAY_BUFFER, cmd.handle);
  }

  void handleCommand(const BindVertexBuffer &cmd) {
    auto current = getCurrentPipeline();
    if (!current) {
      LOG(WARNING) << "Tried to bind vertex buffer without pipeline";
      return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, cmd.handle);

    auto bindAttr = [](const VertexAttribute &attr) {
      glVertexAttribPointer(
          attr.location, attr.size / sizeof(float), GL_FLOAT, GL_FALSE,
          attr.stride, reinterpret_cast<void *>(intptr_t(attr.offset)));
      glEnableVertexAttribArray(attr.location);
    };

    if (cmd.attr_idx) {
      bindAttr(current->layout.attributes[cmd.attr_idx.value()]);
    } else {
      for (const auto &attr : current->layout.attributes) {
        bindAttr(attr);
      }
    }
  }

  void handleCommand(const BindIndexBuffer &cmd) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd.handle);
  }

  void handleCommand(const BindTexture &cmd) {
    glBindTexture(GL_TEXTURE_2D, cmd.handle);
  }

  void handleCommand(const UpdateBuffer &cmd) {
    glBindBuffer(GL_ARRAY_BUFFER, cmd.buffer_handle);
    glBufferSubData(GL_ARRAY_BUFFER, cmd.offset, cmd.data.size(),
                    cmd.data.data());
  }

  std::optional<CompiledPipeline> getCurrentPipeline() {
    if (current_ == 0) {
      return std::nullopt;
    }
    return pipelines_[current_ - 1];
  }

  void handleCommand(const Use &cmd) {
    current_ = cmd.pipeline;
    std::optional<CompiledPipeline> pipeline = getCurrentPipeline();
    glUseProgram(pipeline->program_handle);
    glBindVertexArray(pipeline->vao);
  }

  void handleCommand(const SetUniform &cmd) {
    std::optional<CompiledPipeline> current = getCurrentPipeline();
    if (!current) {
      LOG(WARNING) << "Tried to bind vertex buffer without pipeline";
      return;
    }

    if (cmd.arg_index >= current->layout.uniforms.size()) {
      LOG(WARNING) << "Uniform not found: " << cmd.arg_index;
    }

    GLuint location = glGetUniformLocation(
        current->program_handle,
        current->layout.uniforms[cmd.arg_index].name.c_str());

    if (location != (GLuint)-1) {
      if (cmd.value.size() == sizeof(float)) {
        glUniform1f(location,
                    *reinterpret_cast<const float *>(cmd.value.data()));
      } else if (cmd.value.size() == sizeof(float) * 4) {
        glUniform4fv(location, 1,
                     reinterpret_cast<const float *>(cmd.value.data()));
      } else if (cmd.value.size() == sizeof(float) * 3) {
        glUniform3fv(location, 1,
                     reinterpret_cast<const float *>(cmd.value.data()));
      } else if (cmd.value.size() == sizeof(int32_t)) {
        glUniform1i(location,
                    *reinterpret_cast<const int32_t *>(cmd.value.data()));
      } else if (cmd.value.size() == sizeof(glm::mat4)) {
        glUniformMatrix4fv(location, 1, GL_FALSE,
                           (float *)cmd.value.data());
      } else {
        LOG(WARNING) << "Tried to set unsupported uniform type";
      }
    } else {
      LOG(WARNING) << "Uniform not found: "
                   << current->layout.uniforms[cmd.arg_index].name.c_str();
    }
  }

  void handleCommand(const Draw &cmd) {
    glDrawArrays(primitiveToSys(cmd.primitive), cmd.first_vertex,
                 cmd.vertex_count);
  }

  void handleCommand(const SetViewport &cmd) {
    glViewport(cmd.x, cmd.y, cmd.width, cmd.height);
  }

  void handleCommand(const DrawIndexed &cmd) {
    GLenum primitive = primitiveToSys(cmd.primitive);
    if (cmd.instance_count > 1) {
      glDrawElementsInstanced(primitive, cmd.index_count, GL_UNSIGNED_INT,
                              0, cmd.instance_count);
    } else {
      glDrawElements(primitive, cmd.index_count, GL_UNSIGNED_INT,
                     (void *)(cmd.first_index * sizeof(uint32_t)));
    }
  }
};
