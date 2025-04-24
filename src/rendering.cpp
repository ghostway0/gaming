#include <glm/gtc/type_ptr.hpp>
#include <absl/log/log.h>

#include "sunset/backend.h"
#include "sunset/camera.h"
#include "sunset/geometry.h"
#include "sunset/utils.h"

#include "sunset/rendering.h"

RenderingSystem::RenderingSystem(Backend &backend) {
  initializePipeline(backend);
}

void RenderingSystem::update(ECS &ecs, Camera const &camera,
                             std::vector<Command> &commands) {
  glm::mat4 view = camera.viewMatrix();
  glm::mat4 projection = camera.projectionMatrix();
  commands.push_back(Use{pipeline_handle_});
  commands.push_back(
      SetUniform{.arg_index = 1,
                 .value = to_bytes(std::vector<float>(
                     glm::value_ptr(view), glm::value_ptr(view) + 16))});

  commands.push_back(SetUniform{
      .arg_index = 2,
      .value = to_bytes(std::vector<float>(
          glm::value_ptr(projection), glm::value_ptr(projection) + 16))});

  ecs.forEach(std::function(
      [&](Entity entity, Transform *transform, MeshRenderable *mesh) {
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
