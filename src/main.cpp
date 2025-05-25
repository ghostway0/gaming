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
#include "sunset/fbx.h"
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

bool isConvex(const glm::vec2 &v0, const glm::vec2 &v1,
              const glm::vec2 &v2) {
  glm::vec2 a = v0 - v1;
  glm::vec2 b = v2 - v1;
  float cross = a.x * b.y - a.y * b.x;
  return cross <= 0; // Counter-clockwise order (FBX convention)
}

bool pointInTriangle(const glm::vec2 &p, const glm::vec2 &v0,
                     const glm::vec2 &v1, const glm::vec2 &v2) {
  auto sign = [](const glm::vec2 &p1, const glm::vec2 &p2,
                 const glm::vec2 &p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
  };
  float d1 = sign(p, v0, v1);
  float d2 = sign(p, v1, v2);
  float d3 = sign(p, v2, v0);
  bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
  bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
  return !(has_neg && has_pos);
}

bool isEar(const std::vector<float> &vertices,
           const std::vector<uint32_t> &polygon, size_t prev, size_t i,
           size_t next) {
  glm::vec2 v0(vertices[polygon[prev] * 3],
               vertices[polygon[prev] * 3 + 1]);
  glm::vec2 v1(vertices[polygon[i] * 3], vertices[polygon[i] * 3 + 1]);
  glm::vec2 v2(vertices[polygon[next] * 3],
               vertices[polygon[next] * 3 + 1]);

  if (!isConvex(v0, v1, v2)) {
    return false;
  }

  for (size_t j = 0; j < polygon.size(); ++j) {
    if (j != prev && j != i && j != next) {
      glm::vec2 p(vertices[polygon[j] * 3], vertices[polygon[j] * 3 + 1]);
      if (pointInTriangle(p, v0, v1, v2)) {
        return false;
      }
    }
  }
  return true;
}

std::vector<uint32_t> triangulate(const std::vector<float> &vertices,
                                  const std::vector<int32_t> &indices) {
  std::vector<uint32_t> result;
  std::vector<uint32_t> polygon;

  for (size_t idx = 0; idx < indices.size(); ++idx) {
    int32_t index = indices[idx];
    polygon.push_back(index < 0 ? -(index + 1) : index);

    if (index < 0) {
      while (polygon.size() >= 3) {
        size_t n = polygon.size();
        bool found_ear = false;

        for (size_t i = 0; i < n; ++i) {
          size_t prev = (i == 0) ? n - 1 : i - 1;
          size_t next = (i + 1) % n;
          if (isEar(vertices, polygon, prev, i, next)) {
            result.push_back(polygon[prev]);
            result.push_back(polygon[i]);
            result.push_back(polygon[next]);
            polygon.erase(polygon.begin() + i);
            found_ear = true;
            break;
          }
        }

        if (!found_ear) {
          break;
        }
      }

      polygon.clear();
    }
  }
  return result;
}

void loadSceneToECS(ECS &ecs, const SavedScene &scene, Backend &backend) {
  for (const Model &model : scene.models) {
    Entity entity = ecs.createEntity();

    Transform transform{
        .position = glm::vec3{0.0f},
        .bounding_box = {{0, 0, 0}, {0, 0, 0}},
        .rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
        .scale = 1.0f,
        .dirty = true,
    };

    for (const SavedMesh &saved_mesh : model.meshes) {
      Mesh mesh;

      const auto &v = saved_mesh.vertices;
      const auto &n = saved_mesh.normals;
      const auto &uv = saved_mesh.uvs;

      size_t count = v.size() / 3;
      mesh.vertices.reserve(count);
      AABB bbox;

      for (size_t i = 0; i < count; ++i) {
        glm::vec3 pos{v[i * 3 + 0], v[i * 3 + 1], v[i * 3 + 2]};
        glm::vec3 norm =
            (n.size() >= 3 * (i + 1))
                ? glm::vec3{n[i * 3 + 0], n[i * 3 + 1], n[i * 3 + 2]}
                : glm::vec3{0.0f, 1.0f, 0.0f};
        glm::vec2 texcoord = (uv.size() >= 2 * (i + 1))
                                 ? glm::vec2{uv[i * 2 + 0], uv[i * 2 + 1]}
                                 : glm::vec2{0.0f};

        Vertex vertex{
            .position = pos,
            .normal = norm,
            .uv = texcoord,
            .bone_ids = glm::ivec4{0},
            .bone_weights = glm::vec4{0.0f},
        };
        mesh.vertices.push_back(vertex);
        bbox =
            mesh.vertices.size() == 1 ? AABB{pos, pos} : bbox.extendTo(pos);
      }

      mesh.indices = triangulate(saved_mesh.vertices, saved_mesh.indices);

      mesh.bounding_box = bbox;

      Material material = scene.materials[saved_mesh.material_id];
      std::string texture_path = scene.textures[material.texture_id].src;
      absl::StatusOr<Image> result = loadTextureFromSrc(texture_path);
      std::optional<Image> texture_image;
      if (result.ok()) {
        texture_image = *result;
      } else {
        texture_image = std::nullopt;
      }

      MeshRenderable renderable = compileMesh(backend, mesh, texture_image);

      Entity mesh_entity = ecs.createEntity();
      unused(ecs.addComponents(
          mesh_entity,
          Transform{
              .position = {},
              .bounding_box = bbox,
              .rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
              .scale = 1.0f,
              .parent = entity,
              .dirty = true,
          },
          renderable,
          PhysicsComponent{
              .velocity = {},
              .acceleration = glm::vec3{0.0f},
              .type = PhysicsComponent::Type::Infinite,
          }));
    }

    unused(ecs.addComponents(entity, transform));
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

  std::ifstream file("thing.fbxt1", std::ios::binary);
  absl::StatusOr<PropertyTree> tree = readPropertyTree(file);
  absl::StatusOr<SavedScene> scene = deserializeTree<SavedScene>(*tree);
  assert(scene.ok());

  EventQueue eq;

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

  // eq.subscribe(std::function(
  //     [](Collision const &event) { LOG(INFO) << "collision"; }));

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

  loadSceneToECS(ecs, *scene, backend);

  DamageSystem damage_system(ecs, eq);

  // absl::Status drm = validateLicense("LICENSE");
  // if (!drm.ok()) {
  //   LOG(INFO) << drm.message();
  //   return 1;
  // }

  bool running = true;
  while (running) {
    eq.send(Tick{});
    rendering.update(ecs, commands, true);
    backend.interpret(commands);
    commands.clear();
    physics.update(ecs, eq, 0.166);
    running = io_provider->poll(eq);

    eq.process();
  }

  return 0;
}
