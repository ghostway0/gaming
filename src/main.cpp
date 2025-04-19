#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <absl/log/log.h>
#include <absl/log/initialize.h>
#include <absl/log/log_entry.h>
#include <absl/log/globals.h>

#include "sunset/camera.h"
#include "sunset/ecs.h"
#include "sunset/event_queue.h"

template <typename T>
std::vector<uint8_t> to_bytes(const std::vector<T> &data) {
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());
  return std::vector<uint8_t>(ptr, ptr + data.size() * sizeof(T));
}

template <typename T>
std::span<const uint8_t> to_bytes_view(const T &value) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&value),
                                  sizeof(T));
}

template <typename T>
std::span<const uint8_t> to_bytes_view(std::span<const T> span) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(
      reinterpret_cast<const uint8_t *>(span.data()), span.size_bytes());
}

template <typename T, size_t N>
std::span<const uint8_t> to_bytes_view(const T (&arr)[N]) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(arr),
                                  sizeof(T) * N);
}

template <typename T>
std::span<const uint8_t> to_bytes_view(const std::vector<T> &vec) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(
      reinterpret_cast<const uint8_t *>(vec.data()),
      vec.size() * sizeof(T));
}

struct Tick {
  size_t seq;

  void serialize(std::ostream &os) const { os << seq; }
  static Tick deserialize(std::istream &is) {
    Tick tick;
    is >> tick.seq;
    return tick;
  }
};

using Handle = uint64_t;

struct Use {
  Handle pipeline;
};

struct SetUniform {
  uint32_t arg_index;
  std::vector<uint8_t> value;
};

struct BindBuffer {
  Handle handle;
};

struct BindTexture {
  Handle handle;
};

struct UpdateBuffer {
  Handle buffer_handle;
  size_t offset;
  std::vector<uint8_t> data;
};

struct BindVertexBuffer {
  std::optional<uint32_t> attr_idx;
  Handle handle;
};

struct BindIndexBuffer {
  enum class IndexType { UINT16, UINT32 } type;
  Handle handle;
  size_t offset = 0;
};

struct Draw {
  uint32_t vertex_count;
  uint32_t instance_count = 1;
  uint32_t first_vertex = 0;
  uint32_t first_instance = 0;
};

struct DrawIndexed {
  uint32_t index_count;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

using Command =
    std::variant<BindBuffer, BindVertexBuffer, BindIndexBuffer, BindTexture,
                 UpdateBuffer, Use, SetUniform, Draw, DrawIndexed>;

#include <vector>
#include <variant>
#include <optional>
#include <algorithm>

using Group = std::vector<Command>;

bool isDraw(const Command &c) {
  return std::holds_alternative<Draw>(c) ||
         std::holds_alternative<DrawIndexed>(c);
}

bool isUse(const Command &c) {
  return std::holds_alternative<Use>(c);
}

std::size_t hashGroupState(const Group &g) {
  std::size_t h = 0;
  for (const auto &c : g) {
    if (isDraw(c) || isUse(c)) continue;
    h ^= std::hash<size_t>()(c.index()) + 0x9e3779b9 + (h << 6) + (h >> 2);
  }
  return h;
}

std::vector<Group> splitIntoGroups(const std::vector<Command> &commands) {
  std::vector<Group> groups;
  Group current;
  for (const auto &cmd : commands) {
    if (std::holds_alternative<Use>(cmd)) {
      if (!current.empty()) groups.push_back(std::move(current));
      current = {cmd};
    } else {
      current.push_back(cmd);
    }
  }
  if (!current.empty()) groups.push_back(std::move(current));
  return groups;
}

void minimizeDrawCalls(std::vector<Command> &commands) {
  auto groups = splitIntoGroups(commands);

  std::sort(groups.begin(), groups.end(),
            [](const Group &a, const Group &b) {
              auto getPipeline = [](const Group &g) {
                for (const auto &cmd : g) {
                  if (std::holds_alternative<Use>(cmd))
                    return std::get<Use>(cmd).pipeline;
                }
                return Handle(0);
              };
              auto pa = getPipeline(a), pb = getPipeline(b);
              if (pa != pb) return pa < pb;
              return hashGroupState(a) < hashGroupState(b);
            });

  std::vector<Command> result;
  for (size_t i = 0; i < groups.size(); ++i) {
    const auto &group = groups[i];
    result.insert(result.end(), group.begin(), group.end());

    while (i + 1 < groups.size()) {
      const auto &next = groups[i + 1];
      bool same_pipeline = std::get<Use>(group[0]).pipeline ==
                           std::get<Use>(next[0]).pipeline;
      bool same_state = hashGroupState(group) == hashGroupState(next);

      if (!same_pipeline || !same_state) break;

      for (const auto &cmd : next) {
        if (isDraw(cmd)) result.push_back(cmd);
      }
      i++;
    }
  }

  commands = std::move(result);
}

struct VertexAttribute {
  std::string name;
  uint32_t size;
  uint32_t location;
  uint32_t binding;
  uint64_t offset;
  uint32_t stride;
};

struct Uniform {
  std::string name;
  uint32_t binding;
  uint32_t size;
};

struct PipelineLayout {
  std::vector<VertexAttribute> attributes;
  std::vector<Uniform> uniforms;
};

enum class ShaderType {
  Vertex,
  Fragment,
  Compute,
};

struct Shader {
  ShaderType type;
  std::string source;
  std::string lang;
};

struct Pipeline {
  PipelineLayout layout;
  std::vector<Shader> shaders;
};

class Backend {
  virtual void interpret(std::span<const Command> commands) = 0;

  virtual Handle compile_pipeline(Pipeline pipeline) = 0;

  virtual Handle upload(std::span<const uint8_t> buffer) = 0;
};

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <span>
#include <string>
#include <variant>

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
    glDrawElements(GL_TRIANGLES, cmd.index_count, GL_UNSIGNED_INT,
                   (void *)(cmd.first_index * sizeof(uint32_t)));
  }
};

int main() {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  EventQueue eq;
  eq.subscribe(std::function([](const Tick &) { LOG(INFO) << "hello"; }));
  eq.send(Tick{});
  eq.sendDelayed(Tick{}, absl::Milliseconds(1000));
  eq.process();

  ECS ecs;
  Entity entity = ecs.createEntity();

  [[maybe_unused]]
  bool _ = ecs.addComponents(entity, Tick{}, Camera{{}, {}}).ok();

  ecs.forEach(std::function([](Entity, Camera *camera) {
    LOG(INFO) << camera->position().x << " " << camera->position().y << " "
              << camera->position().z;
  }));
  eq.process();

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW!" << std::endl;
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(800, 600, "OpenGL Backend", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window!" << std::endl;
    return -1;
  }

  glfwMakeContextCurrent(window);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  // glEnable(GL_DEBUG_OUTPUT);

  glewInit();

  OpenGLBackend backend;

  std::vector<VertexAttribute> attr{
      VertexAttribute{
          .name = "aPosition",
          .size = 3 * sizeof(float),
          .location = 0,
          .offset = 0,
          .stride = sizeof(float) * 3,
      },
  };

  Pipeline pipeline = {
      .layout = {.attributes = attr,
                 .uniforms = {Uniform{.name = "uColor",
                                      .binding = 0,
                                      .size = 3 * sizeof(float)}}},
      .shaders = {Shader{ShaderType::Vertex,
                         "#version 330 core\n"
                         "layout(location = 0) in vec3 aPosition;\n"
                         "uniform vec3 uColor;\n"
                         "out vec3 colorOut;\n"
                         "void main() {\n"
                         "  gl_Position = vec4(aPosition, 1.0);\n"
                         "  colorOut = uColor;\n"
                         "}",
                         "glsl"},
                  Shader{ShaderType::Fragment,
                         "#version 330 core\n"
                         "out vec4 FragColor;\n"
                         "in vec3 colorOut;\n"
                         "void main() {\n"
                         "  FragColor = vec4(colorOut, 1.0f);\n"
                         "}",
                         "glsl"}},
  };

  Handle pipeline_handle = backend.compile_pipeline(pipeline);

  std::vector<float> vertex_data = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f,
                                    0.0f,  0.0f,  0.5f, 0.0f};

  std::vector<float> vertex_data2 = {-0.75f, -0.75f, 0.0f,  0.25f, -0.25f,
                                     0.0f,   0.0f,   0.25f, 0.0f};

  Handle vertex_buffer = backend.upload(to_bytes_view(vertex_data));
  Handle vertex_buffer2 = backend.upload(to_bytes_view(vertex_data2));

  std::vector<Command> commands = {
      Use{pipeline_handle},
      BindVertexBuffer{0, vertex_buffer},
      SetUniform{0, to_bytes(std::vector<float>{1.0, 1.0, 1.0})},
      Draw{.vertex_count = 3},

      Use{pipeline_handle},
      BindVertexBuffer{0, vertex_buffer2},
      SetUniform{0, to_bytes(std::vector<float>{1.0, 0.0, 0.0})},
      Draw{.vertex_count = 3},
  };
  // minimizeDrawCalls(commands);

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    backend.interpret(commands);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
