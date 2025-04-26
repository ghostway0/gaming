#include <cassert>
#include <iostream>

#include <absl/log/log.h>
#include <absl/log/initialize.h>
#include <absl/log/log_entry.h>
#include <absl/log/globals.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "sunset/camera.h"
#include "sunset/controller.h"
#include "sunset/ecs.h"
#include "sunset/event_queue.h"
#include "sunset/backend.h"
#include "sunset/geometry.h"
#include "io_provider.cpp"
#include "sunset/physics.h"
#include "sunset/utils.h"
#include "sunset/rendering.h"

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

Mesh createExampleMesh() {
  Mesh mesh;

  mesh.vertices = {
      Vertex{{-0.5f, -0.5f, 0.0f},
             {1.0f, 0.0f, 0.0f},
             {0.5f, 1.0f}}, // Top vertex (red)
      Vertex{{1.0f, 0.0f, 0.0},
             {0.0f, 1.0f, 0.0f},
             {0.0f, 0.0f}}, // Bottom-left (green)
      Vertex{{0.5f, 1.05, 0.0f},
             {0.0f, 0.0f, 1.0f},
             {1.0f, 0.0f}}, // Bottom-right (blue)
  };

  mesh.indices = {0, 1, 2};

  mesh.normal = glm::normalize(
      glm::cross(mesh.vertices[1].position - mesh.vertices[0].position,
                 mesh.vertices[2].position - mesh.vertices[0].position));

  return mesh;
}

int main() {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  EventQueue eq;
  // eq.subscribe(std::function([](const Tick &) { LOG(INFO) << "hello";
  // }));
  eq.send(Tick{});
  eq.sendDelayed(Tick{}, absl::Milliseconds(1000));
  eq.process();

  ECS ecs;

  if (!glfwInit()) {
    LOG(ERROR) << "Failed to init glfw";
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window = glfwCreateWindow(800, 600, "", nullptr, nullptr);
  if (!window) {
    LOG(ERROR) << "Failed to create GLFW window!";
    return -1;
  }

  glfwMakeContextCurrent(window);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  // glEnable(GL_DEBUG_OUTPUT);

  glewInit();

  OpenGLBackend backend;

  Mesh mesh = createExampleMesh();
  MeshRenderable renderable = compileMesh(backend, mesh);

  Entity entity = ecs.createEntity();
  unused(ecs.addComponents(
      entity,
      Transform{.position = {},
                .rotation = {0.0, 0.0, 0.0, 1.0},
                .bounding_box = {{1.0, 0.5, 0.1}, {0.0, 0.0, 0.0}}},
      renderable, PhysicsComponent{.acceleration = {0.0, 0.0, 0.0}, .type = PhysicsComponent::Type::Infinite}));

  std::vector<Command> commands;

  GLFWIO glfwio(window, eq);

  eq.subscribe(std::function(
      [](Collision const &event) { LOG(INFO) << "collision"; }));

  RenderingSystem rendering(backend);

  PhysicsSystem physics = PhysicsSystem::instance();
  physics.moveObject(ecs, entity, {0.0, 0.0, -1.0}, eq);

  Entity camera_entity = ecs.createEntity();
  unused(ecs.addComponents(
      camera_entity,
      Camera{.viewport = {.width = 1000, .height = 500},
             .fov = glm::radians(45.0),
             .aspect = 0.75},
      Transform{.position = {},
                .rotation = {0.0, 0.0, 0.0, 1.0},
                .bounding_box = {{0.5, 0.5, 0.5}, {0.0, 0.0, 0.0}}},
      PhysicsComponent{.acceleration = {0.0, 0.0, 0.0}},
      Player{.speed = 0.01, .sensitivity = 0.005}));

  PlayerController controller(ecs, eq);

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rendering.update(ecs, commands, true);
    backend.interpret(commands);
    commands.clear();
    physics.update(ecs, eq, 0.166);
    glfwio.poll(eq);
    eq.process();
    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
