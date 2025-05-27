#include <cassert>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <optional>
#include <absl/log/log.h>

#include "sunset/physics.h"
#include "sunset/utils.h"

#include "sunset/geometry.h"

glm::mat4 calculateModelMatrix(ECS const &ecs, Entity entity) {
  glm::mat4 model_matrix = glm::mat4(1.0f);
  Entity current = entity;
  while (true) {
    const Transform *t = ecs.getComponent<Transform>(current);
    glm::mat4 local = glm::translate(glm::mat4(1.0f), t->position);
    local *= glm::toMat4(t->rotation);
    local = glm::scale(local, glm::vec3(t->scale));
    model_matrix = local * model_matrix; // PRE-multiply
    if (!t->parent.has_value()) break;
    current = t->parent.value();
  }

  return model_matrix;
}

MeshRenderable compileMesh(Backend &backend, const Mesh &mesh,
                           std::optional<Image> texture_image) {
  std::optional<Handle> texture = std::nullopt;
  if (texture_image.has_value()) {
    texture = backend.uploadTexture(texture_image.value());
  }

  return MeshRenderable{
      .vertex_buffer = backend.upload(to_bytes_view(mesh.vertices)),
      .vertex_count = mesh.vertices.size(),
      .index_buffer = backend.upload(to_bytes_view(mesh.indices)),
      .index_count = mesh.indices.size(),
      .normal = mesh.normal,
      .texture = texture,
  };
}

bool AABB::intersects(const AABB &other) const {
  return (min.x <= other.max.x && max.x >= other.min.x) &&
         (min.y <= other.max.y && max.y >= other.min.y) &&
         (min.z <= other.max.z && max.z >= other.min.z);
}

glm::vec3 AABB::getCenter() const {
  return (min + max) * 0.5f;
}

AABB AABB::extendTo(const glm::vec3 &pos) const {
  return {glm::min(min, pos), glm::max(max, pos)};
}

AABB AABB::subdivideIndex(size_t i, size_t total) const {
  assert(i < total);

  size_t n = static_cast<size_t>(std::cbrt(static_cast<double>(total)));
  if (n * n * n < total) {
    ++n;
  }

  glm::vec3 size = max - min;
  glm::vec3 sub_size = size / static_cast<float>(n);

  size_t iz = i / (n * n);
  size_t iy = (i % (n * n)) / n;
  size_t ix = i % n;

  glm::vec3 new_min = min + glm::vec3(static_cast<float>(ix) * sub_size.x,
                                      static_cast<float>(iy) * sub_size.y,
                                      static_cast<float>(iz) * sub_size.z);
  glm::vec3 new_max = new_min + sub_size;

  new_max = glm::min(new_max, max);

  return {new_min, new_max};
}

bool AABB::contains(const glm::vec3 &point) const {
  return point.x >= min.x && point.x <= max.x && point.y >= min.y &&
         point.y <= max.y && point.z >= min.z && point.z <= max.z;
}

float AABB::getRadius() const {
  return glm::length((max - min) * (sqrtf(2) / 2));
}

AABB AABB::translate(const glm::vec3 &direction) const {
  return AABB{min + direction, max + direction};
}

AABB AABB::rotate(const glm::quat &rotation) const {
  glm::vec3 corners[8] = {
      glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z),
      glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z),
      glm::vec3(min.x, min.y, max.z), glm::vec3(max.x, min.y, max.z),
      glm::vec3(min.x, max.y, max.z), glm::vec3(max.x, max.y, max.z),
  };

  glm::vec3 new_min = rotation * corners[0];
  glm::vec3 new_max = new_min;

  for (int i = 1; i < 8; ++i) {
    glm::vec3 p = rotation * corners[i];
    new_min = glm::min(new_min, p);
    new_max = glm::max(new_max, p);
  }

  return {new_min, new_max};
}

AABB AABB::scale(float factor) const {
  return {min * factor, max * factor};
}

std::ostream &operator<<(std::ostream &os, const AABB &aabb) {
  os << "AABB {\n"
     << "  min: (" << aabb.min.x << ", " << aabb.min.y << ", " << aabb.min.z
     << "),\n"
     << "  max: (" << aabb.max.x << ", " << aabb.max.y << ", " << aabb.max.z
     << ")\n"
     << "}";
  return os;
}

void rotateEntity(ECS &ecs, Entity e) {
  ecs.getComponent<PhysicsComponent>(e);
}

namespace glm {

std::ostream &operator<<(std::ostream &os, const vec3 &vec) {
  os << "vec3(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const vec4 &vec) {
  os << "vec4(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w
     << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const quat &vec) {
  os << "vec4(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w
     << ")";
  return os;
}

} // namespace glm

namespace {

std::vector<uint32_t> triangulate(const std::vector<int32_t> &indices) {
  std::vector<uint32_t> out;
  std::vector<uint32_t> face;

  for (int idx : indices) {
    uint32_t vertex_index =
        static_cast<uint32_t>(idx < 0 ? -(idx + 1) : idx);
    face.push_back(vertex_index);

    if (idx < 0) {
      for (size_t i = 1; i < face.size() - 1; ++i) {
        out.insert(out.end(), {face[0], face[i], face[i + 1]});
      }
      face.clear();
    }
  }

  return out;
}

} // namespace

absl::StatusOr<MeshRenderable> loadSavedMesh(
    MeshRef const &ref, SavedMesh const &saved_mesh,
    std::optional<Image> texture_image, Backend &backend) {
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
    bbox = mesh.vertices.size() == 1 ? AABB{pos, pos} : bbox.extendTo(pos);
  }

  mesh.indices = triangulate(saved_mesh.indices);

  mesh.bounding_box = bbox;

  ResourceManager &rman = ResourceManager::instance();

  std::optional<PropertyTree> material_tree =
      rman.getResource(ref.rref.scope, saved_mesh.material_id);
  if (!material_tree.has_value()) {
    return absl::InvalidArgumentError("Invalid material resource id");
  }

  return compileMesh(backend, mesh, texture_image);
}
