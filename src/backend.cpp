#include "sunset/backend.h"

PipelineBuilder::PipelineBuilder(Backend &backend) : backend_{backend} {}

Handle PipelineBuilder::build() {
  PipelineLayout layout{.attributes = vertex_attrs_, .uniforms = uniforms_};
  Pipeline pipeline{.layout = layout, .shaders = shaders_};
  return backend_.compilePipeline(pipeline);
}

PipelineBuilder &PipelineBuilder::vertexAttr(VertexAttribute attr) {
  vertex_attrs_.push_back(attr);
  return *this;
}

PipelineBuilder &PipelineBuilder::uniform(Uniform uniform) {
  uniforms_.push_back(uniform);
  return *this;
}

PipelineBuilder &PipelineBuilder::shader(Shader shader) {
  shaders_.push_back(shader);
  return *this;
}
