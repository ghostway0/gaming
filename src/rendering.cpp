#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <absl/log/log.h>

#include "sunset/backend.h"
#include "sunset/camera.h"
#include "sunset/geometry.h"
#include "sunset/utils.h"

#include "sunset/rendering.h"

namespace {

std::vector<glm::vec3> getAABBVertices(const AABB &aabb) {
  glm::vec3 min = aabb.min;
  glm::vec3 max = aabb.max;
  return {
      {min.x, min.y, min.z}, {max.x, min.y, min.z}, {max.x, max.y, min.z},
      {min.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z},
      {max.x, max.y, max.z}, {min.x, max.y, max.z},
  };
}

std::vector<uint32_t> getAABBIndices() {
  return {
      0, 1, 1, 2, 2, 3, 3, 0, // bottom face
      4, 5, 5, 6, 6, 7, 7, 4, // top face
      0, 4, 1, 5, 2, 6, 3, 7  // vertical edges
  };
}

} // namespace

DebugOverlay::DebugOverlay(Backend &backend) {
  initializePipeline(backend);
}

void DebugOverlay::initializePipeline(Backend &backend) {
  std::vector<VertexAttribute> debug_pipeline_attr = {
      VertexAttribute{.name = "aPosition",
                      .size = 3 * sizeof(float),
                      .location = 0,
                      .binding = 0,
                      .offset = 0,
                      .stride = sizeof(glm::vec3)},
  };

  std::vector<Uniform> debug_pipeline_uniforms = {
      Uniform{.name = "uModel", .binding = 0, .size = 16 * sizeof(float)},
      Uniform{.name = "uView", .binding = 1, .size = 16 * sizeof(float)},
      Uniform{
          .name = "uProjection", .binding = 2, .size = 16 * sizeof(float)},
  };

  std::vector<Shader> debug_shaders = {
      Shader{.type = ShaderType::Vertex,
             .source = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;
void main() {
  gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
})",
             .lang = "glsl"},
      Shader{.type = ShaderType::Fragment,
             .source = R"(
#version 330 core
out vec4 FragColor;
void main() {
  FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
    )",
             .lang = "glsl"},
  };

  vertex_buffer_ = backend.allocDynamic(3 * 8 * sizeof(float));
  index_buffer_ = backend.allocDynamic(3 * 8 * sizeof(uint32_t));

  PipelineLayout layout = {.attributes = debug_pipeline_attr,
                           .uniforms = debug_pipeline_uniforms};
  aabb_pipeline_ = backend.compilePipeline(
      Pipeline{.layout = layout, .shaders = debug_shaders});
}

void DebugOverlay::update(ECS &ecs, std::vector<Command> &commands) {
  ecs.forEach(std::function([&](Entity entity, Camera *camera,
                                Transform *transform) {
    glm::mat4 view = calculateViewMatrix(camera, transform);
    glm::mat4 projection = calculateProjectionMatrix(camera, transform);

    // commands.push_back(SetViewport{camera->viewport.x,
    // camera->viewport.y,
    //                                camera->viewport.width,
    //                                camera->viewport.height});
    ecs.forEach(std::function([&](Entity entity, Transform *transform,
                                  MeshRenderable *mesh) {
      glm::mat4 model = calculateModelMatrix(ecs, entity);
      const AABB &box = transform->bounding_box;

      std::vector<uint32_t> indices = getAABBIndices();

      commands.push_back(Use{aabb_pipeline_});
      commands.push_back(
          UpdateBuffer{vertex_buffer_, to_bytes(getAABBVertices(box))});
      commands.push_back(UpdateBuffer{index_buffer_, to_bytes(indices)});
      commands.push_back(BindVertexBuffer{.handle = vertex_buffer_});
      commands.push_back(BindIndexBuffer{.handle = index_buffer_});

      commands.push_back(SetUniform{
          .arg_index = 0, // uModel
          .value = to_bytes(std::vector<float>(
              glm::value_ptr(model), glm::value_ptr(model) + 16))});

      commands.push_back(SetUniform{
          .arg_index = 1, // uView
          .value = to_bytes(std::vector<float>(
              glm::value_ptr(view), glm::value_ptr(view) + 16))});

      commands.push_back(SetUniform{.arg_index = 2, // uProjection
                                    .value = to_bytes(std::vector<float>(
                                        glm::value_ptr(projection),
                                        glm::value_ptr(projection) + 16))});

      commands.push_back(DrawIndexed{
          .index_count = static_cast<uint32_t>(indices.size()),
      });
    }));
  }));
}

RenderingSystem::RenderingSystem(Backend &backend)
    : debug_overlay_(backend) {
  initializePipeline(backend);
}

void RenderingSystem::update(ECS &ecs, std::vector<Command> &commands,
                             bool debug) {
  ecs.forEach(std::function([&](Entity entity, Camera *camera,
                                Transform *transform) {
    glm::mat4 view = calculateViewMatrix(camera, transform);
    glm::mat4 projection = calculateProjectionMatrix(camera, transform);

    // commands.push_back(SetViewport{camera->viewport.x,
    // camera->viewport.y,
    //                                camera->viewport.width,
    //                                camera->viewport.height});
    commands.push_back(Use{pipeline_handle_});
    commands.push_back(
        SetUniform{.arg_index = 1,
                   .value = to_bytes(std::vector<float>(
                       glm::value_ptr(view), glm::value_ptr(view) + 16))});

    commands.push_back(SetUniform{
        .arg_index = 2,
        .value = to_bytes(std::vector<float>(
            glm::value_ptr(projection), glm::value_ptr(projection) + 16))});

    ecs.forEach(std::function([&](Entity entity, Transform *transform,
                                  MeshRenderable *mesh) {
      glm::mat4 model = calculateModelMatrix(ecs, entity);

      transform->cached_model = model;

      commands.push_back(Use{pipeline_handle_});
      commands.push_back(BindVertexBuffer{.handle = mesh->vertex_buffer});

      commands.push_back(SetUniform{
          .arg_index = 0,
          .value = to_bytes(std::vector<float>(
              glm::value_ptr(model), glm::value_ptr(model) + 16))});

      // SkeletonComponent *skeleton =
      //     ecs.getComponent<SkeletonComponent>(entity);
      // if (skeleton) {
      //   commands.push_back(SetUniform{
      //       .arg_index = 3,
      //       .value = to_bytes(skeleton->final_transforms),
      //   });
      // }

      commands.push_back(Draw{
          .vertex_count = static_cast<uint32_t>(mesh->vertex_count),
      });
    }));
  }));

  if (debug) {
    debug_overlay_.update(ecs, commands);
  }
}

void RenderingSystem::initializePipeline(Backend &backend) {
  std::vector<VertexAttribute> attributes = {
      VertexAttribute{.name = "aPosition",
                      .size = 3 * sizeof(float),
                      .location = 0,
                      .binding = 0,
                      .offset = offsetof(Vertex, position),
                      .stride = sizeof(Vertex)},
      VertexAttribute{.name = "aNormal",
                      .size = 3 * sizeof(float),
                      .location = 1,
                      .binding = 0,
                      .offset = offsetof(Vertex, normal),
                      .stride = sizeof(Vertex)},
      VertexAttribute{.name = "aUV",
                      .size = 2 * sizeof(float),
                      .location = 2,
                      .binding = 0,
                      .offset = offsetof(Vertex, uv),
                      .stride = sizeof(Vertex)},
  };

  std::vector<Uniform> uniforms = {
      Uniform{.name = "uModel", .binding = 0, .size = 16 * sizeof(float)},
      Uniform{.name = "uView", .binding = 1, .size = 16 * sizeof(float)},
      Uniform{
          .name = "uProjection", .binding = 2, .size = 16 * sizeof(float)},
      Uniform{.name = "uBoneTransforms",
              .binding = 3,
              .size = 64 * 16 * sizeof(float)},
  };

  std::vector<Shader> shaders = {
      Shader{.type = ShaderType::Vertex,
             .source = R"(
                    #version 330 core
                    layout(location = 0) in vec3 aPosition;
                    layout(location = 1) in vec3 aNormal;
                    layout(location = 2) in vec2 aUV;
                    layout(location = 3) in uvec4 aBoneIndices;
                    layout(location = 4) in vec4 aBoneWeights;
                    uniform mat4 uModel;
                    uniform mat4 uView;
                    uniform mat4 uProjection;
                    // uniform mat4[64] uBoneTransforms;
                    out vec3 fragNormal;
                    out vec2 fragUV;
                    void main() {
                      // mat4 skin_matrix =
                      //     aBoneWeights.x * uBoneTransforms[aBoneIndices.x] +
                      //     aBoneWeights.y * uBoneTransforms[aBoneIndices.y] +
                      //     aBoneWeights.z * uBoneTransforms[aBoneIndices.z] +
                      //     aBoneWeights.w * uBoneTransforms[aBoneIndices.w];
                      // vec4 skinned_position = skin_matrix * vec4(aPosition, 1.0);
                      gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
                      fragNormal = mat3(uModel) * aNormal;
                      fragUV = aUV;
                    }
                )",
             .lang = "glsl"},
      Shader{.type = ShaderType::Fragment,
             .source = R"(
                    #version 330 core
                    in vec3 fragNormal;
                    in vec2 fragUV;
                    out vec4 FragColor;
                    void main() {
                      FragColor = vec4(1.0);
                    }
                )",
             .lang = "glsl"},
  };

  PipelineLayout layout = {.attributes = attributes, .uniforms = uniforms};
  Pipeline pipeline = {.layout = layout, .shaders = shaders};

  pipeline_handle_ = backend.compilePipeline(pipeline);
}
