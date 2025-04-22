#include <glm/gtc/type_ptr.hpp>

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
        commands.push_back(
            BindVertexBuffer{.attr_idx = 0, .handle = mesh->vertex_buffer});
        commands.push_back(BindIndexBuffer{.handle = mesh->index_buffer});
        commands.push_back(SetUniform{
            .arg_index = 0,
            .value = to_bytes(std::vector<float>(
                glm::value_ptr(model), glm::value_ptr(model) + 16))});
        commands.push_back(DrawIndexed{
            .index_count = static_cast<uint32_t>(mesh->index_count),
            .instance_count = 1});
      }));
}

void RenderingSystem::initializePipeline(Backend &backend) {
  std::vector<VertexAttribute> attributes = {
      VertexAttribute{.name = "aPosition",
                      .size = 3 * sizeof(float),
                      .location = 0,
                      .offset = offsetof(Vertex, position),
                      .stride = sizeof(Vertex)},
      VertexAttribute{.name = "aNormal",
                      .size = 3 * sizeof(float),
                      .location = 1,
                      .offset = offsetof(Vertex, normal),
                      .stride = sizeof(Vertex)},
      VertexAttribute{.name = "aUV",
                      .size = 2 * sizeof(float),
                      .location = 2,
                      .offset = offsetof(Vertex, uv),
                      .stride = sizeof(Vertex)},
  };

  std::vector<Uniform> uniforms = {
      Uniform{.name = "uModel", .binding = 0, .size = 16 * sizeof(float)},
      Uniform{.name = "uView", .binding = 1, .size = 16 * sizeof(float)},
      Uniform{
          .name = "uProjection", .binding = 2, .size = 16 * sizeof(float)},
  };

  std::vector<Shader> shaders = {
      Shader{.type = ShaderType::Vertex,
             .source = R"(
                    #version 330 core
                    layout(location = 0) in vec3 aPosition;
                    layout(location = 1) in vec3 aNormal;
                    layout(location = 2) in vec2 aUV;
                    uniform mat4 uModel;
                    uniform mat4 uView;
                    uniform mat4 uProjection;
                    out vec3 fragNormal;
                    out vec2 fragUV;
                    void main() {
                        gl_Position = vec4(aPosition, 1.0);
                        fragNormal = aNormal;
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
                        FragColor = vec4(normalize(fragNormal), 1.0);
                    }
                )",
             .lang = "glsl"},
  };

  PipelineLayout layout = {.attributes = attributes, .uniforms = uniforms};
  Pipeline pipeline = {.layout = layout, .shaders = shaders};

  pipeline_handle_ = backend.compile_pipeline(pipeline);
}
