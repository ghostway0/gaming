#include <cassert>
#include <fstream>
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
#include "sunset/property_tree.h"
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

void loadSceneToECS(ECS &ecs, const SavedScene &scene, Backend &backend) {
  ResourceManager &rman = ResourceManager::instance();

  for (const PropertyTree &resource : scene.resources) {
    rman.addResource(scene.scope, resource);
  }

  for (const Instance &instance : scene.instances) {
    Entity entity = ecs.createEntity();

    for (const PropertyTree &component_tree : instance.components) {
      std::optional<ComponentRegistry::DeserializeFn> des_fn =
          ComponentRegistry::instance().getDeserializer(
              component_tree.name);

      if (!des_fn.has_value()) {
        LOG(WARNING) << "Component " << component_tree.name
                     << " is not registered yet.";
        continue;
      }

      absl::StatusOr<Any> component = des_fn.value()(component_tree);
      if (!component.ok()) {
        continue;
      }

      LOG(INFO) << entity << " " << component_tree.name;
      ecs.addComponentRaw(entity, std::move(*component));
    }
  }
}

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

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"DamageComponent"};
    return tree;
  }

  static absl::StatusOr<DamageComponent> deserialize(
      PropertyTree const & /* tree */) {
    return absl::InternalError("TODO");
  }
};

struct Health {
  float amount;
  float damage_mult = 1.0;

  std::optional<PropertyTree> serialize() const {
    // TODO:
    PropertyTree tree = {"Health"};
    return tree;
  }

  static absl::StatusOr<Health> deserialize(
      PropertyTree const & /* tree */) {
    return absl::InternalError("TODO");
  }
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

  // HACK:
  ComponentRegistry::instance().registerType<MeshRef>();
  ComponentRegistry::instance().registerType<TextureRef>();

  std::ifstream file("world.pt", std::ios::binary);
  absl::StatusOr<PropertyTree> tree = readPropertyTree(file);
  absl::StatusOr<SavedScene> scene = deserializeTree<SavedScene>(*tree);
  assert(scene.ok());

  EventQueue eq;

  ECS ecs;

  std::unique_ptr<IOProvider> io_provider = std::make_unique<GLFWIO>(eq);
  assert(io_provider->valid());

  OpenGLBackend backend;

  // Mesh mesh = createExampleMesh();
  // MeshRenderable renderable = compileMesh(backend, mesh);
  //
  // Entity entity = ecs.createEntity();
  // unused(
  //     ecs.addComponents(entity,
  //                       Transform{
  //                           .position = {},
  //                           .rotation = {0.0, 0.0, 0.0, 1.0},
  //                       },
  //                       renderable,
  //                       PhysicsComponent{
  //                           .acceleration = {0.0, 0.0, 0.0},
  //                           .type = PhysicsComponent::Type::Infinite,
  //                           .collider = {{0.0, 0.0, 0.0}, {1.0, 0.5,
  //                           0.1}},
  //                       },
  //                       Health{100.0, 2.0}));

  std::vector<Command> commands;

  eq.subscribe(std::function([](Collision const &event) {
    LOG(INFO) << "collision between " << event.entity_a << " and "
              << event.entity_b;
  }));

  RenderingSystem rendering(backend);

  PhysicsSystem physics = PhysicsSystem::instance();
  // physics.moveObject(ecs, entity, {0.0, 0.0, -1.0}, eq);

  Entity camera_entity = ecs.createEntity();
  unused(ecs.addComponents(
      camera_entity,
      Camera{.viewport = {.width = 1000, .height = 500},
             .fov = glm::radians(45.0),
             .aspect = 0.75},
      Transform{.position = {0.0, 1.0, 0.0}, .rotation = glm::quat()},
      PhysicsComponent{
          .acceleration = {0.0, -0.01, 0.0},
          .type = PhysicsComponent::Type::Regular,
          .material = {.restitution = 0.0},
          .collider = AABB{{-0.2, -0.5, -0.2}, {0.2, 0.2, 0.2}}.translate(
              {0.0, 1.0, 0.0}),
          .collision_source = camera_entity,
      },
      Player{.speed = 0.01, .sensitivity = 0.005}));

  PlayerController controller(ecs, eq);

  eq.subscribe(std::function([&](const MouseDown &event) {
    Entity bullet = ecs.createEntity();

    Transform *camera_transform =
        ecs.getComponent<Transform>(camera_entity);

    Transform bullet_transform{.position = camera_transform->position,
                               .rotation = camera_transform->rotation};

    glm::vec3 forward = glm::normalize(
        glm::vec3(camera_transform->rotation * glm::vec4(0, 0, -1, 0.0f)));

    unused(ecs.addComponents(
        bullet, bullet_transform,
        PhysicsComponent{
            .velocity = forward * 0.01f,
            .type = PhysicsComponent::Type::Regular,
            .material = {},
            .collider = AABB{{-0.0005f, -0.0005f, -0.0005f},
                             {0.0005f, 0.0005f, 0.0005f}}
                            .translate(camera_transform->position),
        },
        DamageComponent{4.0}));

    LOG(INFO) << "Bullet spawned!";
  }));

  loadSceneToECS(ecs, *scene, backend);

  DamageSystem damage_system(ecs, eq);

  // absl::Status drm = validateLicense("LICENSE");
  // if (!drm.ok()) {
  //   LOG(INFO) << drm.message();
  //   return 1;
  // }

  compileScene(ecs, backend);

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
