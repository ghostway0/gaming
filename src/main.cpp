#include <cassert>
#include <glm/trigonometric.hpp>
#include <iostream>

#include <absl/log/log.h>
#include <absl/log/initialize.h>
#include <absl/log/log_entry.h>
#include <absl/log/globals.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "sunset/camera.h"
#include "sunset/ecs.h"
#include "sunset/event_queue.h"
#include "sunset/backend.h"
#include "sunset/geometry.h"
#include "io_provider.cpp"
#include "sunset/physics.h"
#include "sunset/property_tree.h"
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

struct Vector3 {
  double x, y, z;
};

template <>
struct TypeDeserializer<Vector3> {
  static std::vector<FieldDescriptor<Vector3>> getFields() {
    return {
        makeSetter("x", &Vector3::x),
        makeSetter("y", &Vector3::y),
        makeSetter("z", &Vector3::z),
    };
  }
};

#include <fstream>

absl::StatusOr<Vector3> parseVector3(const std::string &filename) {
  std::ifstream input(filename, std::ios::binary);
  auto node_opt = readPropertyTree(input);
  LOG(INFO) << *node_opt;
  return deserializeNode<Vector3>(*node_opt);
}

int main() {
  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  Vector3 a = parseVector3("data.bin").value();
  LOG(INFO) << a.x << " " << a.y << " " << a.z;

  EventQueue eq;
  eq.subscribe(std::function([](const Tick &) { LOG(INFO) << "hello"; }));
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
  Transform t = {
      .position = {0.0, 0.0, -1.0},
      .bounding_box = {},
      .rotation = {0.0, 0.0, 0.0},
  };

  Entity entity = ecs.createEntity();
  PhysicsComponent physics_comp{.acceleration = {0.0, -0.001, 0.0}};
  unused(ecs.addComponents(entity, t, renderable, physics_comp));

  std::vector<Command> commands;

  GLFWIO glfwio(window, eq);

  // eq.subscribe(std::function([](KeyPressed const &event) {
  //   LOG(INFO) << event.map[static_cast<size_t>(Key::W)];
  // }));

  RenderingSystem rendering(backend);

  Camera camera(Camera::State{.up = {0.0, 1.0, 0.0}},
                Camera::Options{
                    .fov = glm::radians(45.0),
                    .sensitivity = 1.0,
                    .speed = 1.0,
                    .aspect_ratio = 0.75,
                });

  PhysicsSystem physics = PhysicsSystem::instance();
  physics.moveObject(ecs, entity, {0.0, 0.0, -1.0}, eq);

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rendering.update(ecs, camera, commands);
    backend.interpret(commands);
    commands.clear();
    physics.update(ecs, eq, 0.166);
    glfwio.poll(eq);
    eq.process();
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
