#include "sunset/backend.h"

PipelineBuilder::PipelineBuilder(Backend &backend) : backend_{backend} {}

Pipeline PipelineBuilder::build() {
  PipelineLayout layout{.attributes = vertex_attrs_, .uniforms = uniforms_};
  Handle handle = backend_.compilePipeline(layout, shaders_);
  return Pipeline{handle, emit_fn_};
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
