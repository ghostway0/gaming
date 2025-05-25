#include <cassert>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <absl/log/log.h>

#include "sunset/backend.h"
#include "sunset/camera.h"
#include "sunset/geometry.h"
#include "sunset/image.h"
#include "sunset/physics.h"
#include "sunset/utils.h"
#include "sunset/globals.h"

#include "sunset/rendering.h"

const static std::string kTextVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main() {
  gl_Position = vec4(aPos.x, -aPos.y, 0.0, 1.0);
  vUV = aUV.xy;
})";

const static std::string kTextFragmentShader = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uFont;
out vec4 FragColor;

void main() {
  float intensity = texture(uFont, vUV).r;
  FragColor = vec4(intensity);
})";

const static std::string kAABBDebugVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;
void main() {
  gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
)";

const static std::string kAABBDebugFragmentShader = R"(
#version 330 core
out vec4 FragColor;
void main() {
  FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)";

DebugOverlay::DebugOverlay(Backend &backend) {
  last_frame_ = absl::Now();
  font_ = loadPSF2Font("debug-font.psf2").value();
  initializePipeline(backend);
}

void DebugOverlay::initializePipeline(Backend &backend) {
  Handle aabb_vertex_buf = backend.allocDynamic(3 * 8 * sizeof(float));
  Handle aabb_index_buf = backend.allocDynamic(3 * 8 * sizeof(uint32_t));

  auto aabbEmitFn = [=](std::vector<Command> &commands, glm::mat4 proj,
                        glm::mat4 model, glm::mat4 view, AABB box) {
    std::array<uint32_t, 24> indices = {
        0, 1, 1, 2, 2, 3, 3, 0, // bottom face
        4, 5, 5, 6, 6, 7, 7, 4, // top face
        0, 4, 1, 5, 2, 6, 3, 7  // vertical edges
    };

    std::vector<glm::vec3> vertices = {
        {box.min.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.min.z},
        {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.max.x, box.min.y, box.max.z},
        {box.max.x, box.max.y, box.max.z},
        {box.min.x, box.max.y, box.max.z},
    };

    commands.push_back(UpdateBuffer{aabb_vertex_buf, to_bytes(vertices)});
    commands.push_back(UpdateBuffer{aabb_index_buf, to_bytes(indices)});
    commands.push_back(BindVertexBuffer{.handle = aabb_vertex_buf});
    commands.push_back(BindIndexBuffer{.handle = aabb_index_buf});

    commands.push_back(SetUniform{
        .arg_index = 0, // uModel
        .value = to_bytes(std::vector<float>(glm::value_ptr(model),
                                             glm::value_ptr(model) + 16))});

    commands.push_back(
        SetUniform{.arg_index = 1, // uView
                   .value = to_bytes(std::vector<float>(
                       glm::value_ptr(view), glm::value_ptr(view) + 16))});

    commands.push_back(
        SetUniform{.arg_index = 2, // uProjection
                   .value = to_bytes(std::vector<float>(
                       glm::value_ptr(proj), glm::value_ptr(proj) + 16))});

    commands.push_back(DrawIndexed{
        .index_count = static_cast<uint32_t>(indices.size()),
        .primitive = PrimitiveTopology::Lines,
    });
  };

  aabb_pipeline_ =
      PipelineBuilder(backend)
          .uniform(Uniform{
              .name = "uModel", .binding = 0, .size = 16 * sizeof(float)})
          .uniform(Uniform{
              .name = "uView", .binding = 1, .size = 16 * sizeof(float)})
          .uniform(Uniform{.name = "uProjection",
                           .binding = 2,
                           .size = 16 * sizeof(float)})
          .vertexAttr(VertexAttribute{.name = "aPos",
                                      .size = sizeof(glm::vec3),
                                      .location = 0,
                                      .binding = 0,
                                      .offset = 0,
                                      .stride = sizeof(glm::vec3)})
          .shader(Shader{.type = ShaderType::Vertex,
                         .source = kAABBDebugVertexShader,
                         .lang = "glsl"})
          .shader(Shader{.type = ShaderType::Fragment,
                         .source = kAABBDebugFragmentShader,
                         .lang = "glsl"})
          .emitFn(std::function(aabbEmitFn))
          .build();

  Handle text_vertex_buf = backend.allocDynamic(4096 * sizeof(Vertex));
  Handle text_index_buf = backend.allocDynamic(4096 * sizeof(uint32_t));
  Image atlas = createFontAtlas(font_);
  Handle font_texture = backend.uploadTexture(atlas);

  text_pipeline_ =
      PipelineBuilder(backend)
          .vertexAttr(VertexAttribute{.name = "aPos",
                                      .size = sizeof(glm::vec3),
                                      .location = 0,
                                      .binding = 0,
                                      .offset = offsetof(Vertex, position),
                                      .stride = sizeof(Vertex)})
          .vertexAttr(VertexAttribute{.name = "aUV",
                                      .size = sizeof(glm::vec2),
                                      .location = 1,
                                      .binding = 0,
                                      .offset = offsetof(Vertex, uv),
                                      .stride = sizeof(Vertex)})
          .shader(Shader{ShaderType::Vertex, kTextVertexShader, "glsl"})
          .shader(Shader{ShaderType::Fragment, kTextFragmentShader, "glsl"})
          .emitFn(std::function([this, text_index_buf, text_vertex_buf,
                                 atlas, font_texture](
                                    std::vector<Command> &commands,
                                    std::string text, float x, float y,
                                    float scale = 1.0f) {
            glm::ivec2 screen_size = kScreenSize::get();

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            size_t glyph_w = font_.glyph_sizes.x;
            size_t glyph_h = font_.glyph_sizes.y;

            float char_width =
                scale * static_cast<float>(glyph_w) / screen_size.x;
            float char_height =
                scale * static_cast<float>(glyph_h) / screen_size.y;

            for (size_t i = 0; i < text.size(); i++) {
              uint32_t codepoint = static_cast<uint8_t>(text[i]);

              std::optional<size_t> glyph_index =
                  font_.findGlyphIndex(codepoint);
              if (!glyph_index.has_value()) {
                continue;
              }

              size_t cols = atlas.w() / font_.glyph_sizes.x;

              size_t col = *glyph_index % cols;
              size_t row = *glyph_index / cols;

              float xpos = x + i * char_width;
              float ypos = y;

              float u = static_cast<float>(col * glyph_w) / atlas.w();
              float v = static_cast<float>(row * glyph_h) / atlas.h();
              float u1 =
                  static_cast<float>((col + 1) * glyph_w) / atlas.w();
              float v1 =
                  static_cast<float>((row + 1) * glyph_h) / atlas.h();

              vertices.push_back(
                  Vertex{.position = {xpos, ypos, 0.0}, .uv = {u, v}});
              vertices.push_back(
                  Vertex{.position = {xpos + char_width, ypos, 0.0},
                         .uv = {u1, v}});
              vertices.push_back(Vertex{
                  .position = {xpos + char_width, ypos + char_height, 0.0},
                  .uv = {u1, v1}});
              vertices.push_back(
                  Vertex{.position = {xpos, ypos + char_height, 0.0f},
                         .uv = {u, v1}});

              uint32_t base = static_cast<uint32_t>(i * 4);
              indices.insert(indices.end(), {base, base + 1, base + 2, base,
                                             base + 2, base + 3});
            }

            commands.push_back(
                UpdateBuffer{text_vertex_buf, to_bytes(vertices)});
            commands.push_back(
                UpdateBuffer{text_index_buf, to_bytes(indices)});
            commands.push_back(BindVertexBuffer{.handle = text_vertex_buf});
            commands.push_back(BindIndexBuffer{.handle = text_index_buf});
            commands.push_back(BindTexture{font_texture});
            commands.push_back(DrawIndexed{
                .index_count = static_cast<uint32_t>(indices.size()),
                .primitive = PrimitiveTopology::Triangles});
          }))
          .build();
}

void DebugOverlay::update(ECS &ecs, std::vector<Command> &commands) {
  absl::Time now = absl::Now();
  absl::Duration frame_time = now - last_frame_;
  last_frame_ = now;

  ecs.forEach(std::function(
      [&](Entity entity, Camera *camera, Transform *transform) {
        glm::mat4 view = calculateViewMatrix(camera, transform);
        glm::mat4 projection = calculateProjectionMatrix(camera, transform);

        // commands.push_back(SetViewport{camera->viewport.x,
        // camera->viewport.y,
        //                                camera->viewport.width,
        //                                camera->viewport.height});
        ecs.forEach(std::function([&](Entity entity, PhysicsComponent *physics) {
          glm::mat4 model = glm::mat4(1.0);
          const AABB &box = physics->collider;

          aabb_pipeline_(commands, projection, model, view, box);
        }));
      }));

  text_pipeline_(commands,
                 std::string(absl::StrFormat(
                     "fps: %lu", absl::Seconds(1) / frame_time)),
                 -0.9f, -0.9f, 2.0f);
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

      if (mesh->index_buffer) {
        commands.push_back(BindIndexBuffer{.handle = mesh->index_buffer});
      }

      commands.push_back(SetUniform{
          .arg_index = 0,
          .value = to_bytes(std::vector<float>(
              glm::value_ptr(model), glm::value_ptr(model) + 16))});

      if (mesh->texture.has_value()) {
        commands.push_back(SetUniform{4, to_bytes(0)});
        commands.push_back(BindTexture{mesh->texture.value()});
      } else {
        // commands.push_back(BindTexture{0});
      }

      // SkeletonComponent *skeleton =
      //     ecs.getComponent<SkeletonComponent>(entity);
      // if (skeleton) {
      //   commands.push_back(SetUniform{
      //       .arg_index = 3,
      //       .value = to_bytes(skeleton->final_transforms),
      //   });
      // }

      if (mesh->index_buffer) {
        commands.push_back(DrawIndexed{
            .index_count = static_cast<uint32_t>(mesh->index_count)});
      } else {
        commands.push_back(Draw{
            .vertex_count = static_cast<uint32_t>(mesh->vertex_count)});
      }
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
      Uniform{.name = "uTexture", .binding = 4, .size = sizeof(int)},
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

            uniform sampler2D uTexture;

            void main() {
              FragColor = texture(uTexture, fragUV);
            }
        )",
             .lang = "glsl"},
  };

  PipelineLayout layout = {.attributes = attributes, .uniforms = uniforms};
  pipeline_handle_ = backend.compilePipeline(layout, shaders);
}
