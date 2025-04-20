#include <cassert>
#include <iostream>

#include <absl/log/log.h>
#include <absl/log/initialize.h>
#include <absl/log/log_entry.h>
#include <absl/log/globals.h>

#include "sunset/camera.h"
#include "sunset/ecs.h"
#include "sunset/geometry.h"
#include "sunset/event_queue.h"
#include "sunset/backend.h"
#include "sunset/utils.h"

#include "opengl_backend.cpp"

struct Tick {
  size_t seq;

  void serialize(std::ostream &os) const { os << seq; }
  static Tick deserialize(std::istream &is) {
    Tick tick;
    is >> tick.seq;
    return tick;
  }
};

using Group = std::vector<Command>;

bool isDraw(const Command &c) {
  return std::holds_alternative<Draw>(c) ||
         std::holds_alternative<DrawIndexed>(c);
}

bool isUse(const Command &c) {
  return std::holds_alternative<Use>(c);
}

struct Transform {
  // relative to parent
  glm::vec3 position;
  AABB bounding_box;

  glm::mat4 cached_model;
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

  unused(ecs.addComponents(entity, Tick{}, Camera{{}, {}}));

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

  std::vector<float> vertex_data2 = {-0.75f, -0.75f, 0.0f,  0.75f, -0.75f,
                                     0.0f,   0.0f,   0.75f, 0.0f};

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
