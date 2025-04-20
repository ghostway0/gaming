#include <iostream>
#include <vector>
#include <span>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <absl/log/log.h>
#include <glm/glm.hpp>

#include "sunset/backend.h"

class OpenGLBackend : public Backend {
 public:
  void interpret(std::span<const Command> commands) override {
    for (const auto &command : commands) {
      std::visit([this](const auto &cmd) { this->handle_command(cmd); },
                 command);
    }
  }

  Handle compile_pipeline(Pipeline pipeline) override {
    GLuint program = glCreateProgram();

    std::vector<GLuint> shader_handles;
    for (const Shader &shader : pipeline.shaders) {
      GLuint handle = compile_shader(shader);
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

 private:
  struct CompiledPipeline {
    GLuint program_handle;
    PipelineLayout layout;
    GLuint vao;
  };

  std::vector<CompiledPipeline> pipelines_;
  Handle current_;

  GLuint compile_shader(Shader const &shader) {
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

  void handle_command(const BindBuffer &cmd) {
    glBindBuffer(GL_ARRAY_BUFFER, cmd.handle);
  }

  void handle_command(const BindVertexBuffer &cmd) {
    std::optional<CompiledPipeline> current = get_current_pipeline();
    if (!current) {
      LOG(WARNING) << "Tried to bind vertex buffer without pipeline";
      return;
    }

    VertexAttribute const &attr =
        current->layout.attributes[cmd.attr_idx.value()];

    glBindBuffer(GL_ARRAY_BUFFER, cmd.handle);
    glVertexAttribPointer(attr.location, attr.size / sizeof(float),
                          GL_FLOAT, GL_FALSE, attr.stride,
                          (void *)(intptr_t)attr.offset);
    glEnableVertexAttribArray(attr.location);
    // glBindVertexArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, cmd.handle);
  }

  void handle_command(const BindIndexBuffer &cmd) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd.handle);
  }

  void handle_command(const BindTexture &cmd) {
    glBindTexture(GL_TEXTURE_2D, cmd.handle);
  }

  void handle_command(const UpdateBuffer &cmd) {
    glBindBuffer(GL_ARRAY_BUFFER, cmd.buffer_handle);
    glBufferSubData(GL_ARRAY_BUFFER, cmd.offset, cmd.data.size(),
                    cmd.data.data());
  }

  std::optional<CompiledPipeline> get_current_pipeline() {
    if (current_ == 0) {
      return std::nullopt;
    }
    return pipelines_[current_ - 1];
  }

  void handle_command(const Use &cmd) {
    current_ = cmd.pipeline;
    std::optional<CompiledPipeline> pipeline = get_current_pipeline();
    glUseProgram(pipeline->program_handle);
    glBindVertexArray(pipeline->vao);
  }

  void handle_command(const SetUniform &cmd) {
    std::optional<CompiledPipeline> current = get_current_pipeline();
    if (!current) {
      LOG(WARNING) << "Tried to bind vertex buffer without pipeline";
      return;
    }

    GLuint location = glGetUniformLocation(
        current->program_handle,
        current->layout.uniforms[cmd.arg_index].name.c_str());

    if (location != -1) {
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

  void handle_command(const Draw &cmd) {
    glDrawArrays(GL_TRIANGLES, cmd.first_vertex, cmd.vertex_count);
  }

  void handle_command(const DrawIndexed &cmd) {
    if (cmd.instance_count > 1) {
      glDrawElementsInstanced(GL_TRIANGLES, cmd.index_count,
                              GL_UNSIGNED_INT, 0, cmd.instance_count);
    } else {
      glDrawElements(GL_TRIANGLES, cmd.index_count, GL_UNSIGNED_INT,
                     (void *)(cmd.first_index * sizeof(uint32_t)));
    }
  }
};
