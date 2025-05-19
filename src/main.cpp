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
#include "sunset/drm.h"
#include "sunset/globals.h"
#include "sunset/physics.h"
#include "sunset/utils.h"
#include "sunset/rendering.h"
#include "sunset/opengl_backend.h"
#include "sunset/glfw_provider.h"

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

struct DamageComponent {
  float amount;
  bool used;

  void serialize(std::ostream &os) const {}

  static DamageComponent deserialize(std::istream &is) { return {}; }
};

struct Health {
  float amount;
  float damage_mult = 1.0;

  void serialize(std::ostream &os) const {}

  static Health deserialize(std::istream &is) { return {}; }
};

class DamageSystem {
 public:
  DamageSystem(ECS &ecs, EventQueue &event_queue) {
    event_queue.subscribe(
        std::function([&](EnterCollider const &collision) {
          Entity a = collision.entity;
          Entity b = collision.collider;

          DamageComponent *damage = ecs.getComponent<DamageComponent>(b);
          Health *health = ecs.getComponent<Health>(a);

          if (damage && health && !damage->used) {
            health->amount -= damage->amount * health->damage_mult;
            damage->used = true;
            LOG(INFO) << "current health: " << health->amount;
          }
        }));
  }
};

int main(int argc, char **argv) {
  kCurrentExec::set(argv[0]);

  absl::InitializeLog();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  EventQueue eq;
  // eq.subscribe(std::function([](const Tick &) { LOG(INFO) << "hello";
  // }));
  eq.send(Tick{});
  eq.sendDelayed(Tick{}, absl::Milliseconds(1000));
  eq.process();

  ECS ecs;

  std::unique_ptr<IOProvider> io_provider = std::make_unique<GLFWIO>(eq);
  assert(io_provider->valid());

  OpenGLBackend backend;

  Mesh mesh = createExampleMesh();
  MeshRenderable renderable = compileMesh(backend, mesh);

  Entity entity = ecs.createEntity();
  unused(ecs.addComponents(
      entity,
      Transform{
          .position = {},
          .bounding_box = {{0.0, 0.0, 0.0}, {1.0, 0.5, 0.1}},
          .rotation = {0.0, 0.0, 0.0, 1.0},
      },
      renderable,
      PhysicsComponent{.acceleration = {0.0, 0.0, 0.0},
                       .type = PhysicsComponent::Type::Infinite},
      Health{100.0, 2.0}));

  std::vector<Command> commands;

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
                // .bounding_box = {{0.0, 0.0, 0.0}, {0.1, 0.5, 0.5}},
                .rotation = {0.0, 0.0, 0.0, 1.0}},
      PhysicsComponent{.acceleration = {0.0, 0.0, 0.0}},
      Player{.speed = 0.01, .sensitivity = 0.005}));

  FreeController controller(ecs, eq);

  eq.subscribe(std::function([&](const MouseDown &event) {
    Entity bullet = ecs.createEntity();

    Transform *camera_transform =
        ecs.getComponent<Transform>(camera_entity);

    Transform bullet_transform{
        .position = camera_transform->position,
        .bounding_box = AABB{{-0.0005f, -0.0005f, -0.0005f},
                             {0.0005f, 0.0005f, 0.0005f}}
                            .translate(camera_transform->position),
        .rotation = camera_transform->rotation};

    glm::vec3 forward = glm::normalize(
        glm::vec3(camera_transform->rotation * glm::vec4(0, 0, -1, 0.0f)));

    unused(ecs.addComponents(
        bullet, bullet_transform,
        PhysicsComponent{.velocity = forward * 0.01f,
                         .type = PhysicsComponent::Type::Regular,
                         .material = {}},
        DamageComponent{4.0}));

    LOG(INFO) << "Bullet spawned!";
  }));

  DamageSystem damage_system(ecs, eq);

  // absl::Status drm = validateLicense("LICENSE");
  // if (!drm.ok()) {
  //   LOG(INFO) << drm.message();
  //   return 1;
  // }

  bool running = true;
  while (running) {
    rendering.update(ecs, commands, true);
    backend.interpret(commands);
    commands.clear();
    physics.update(ecs, eq, 0.166);
    running = io_provider->poll(eq);

    eq.process();
  }

  return 0;
}
