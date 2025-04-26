#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <span>

using Handle = uint64_t;

struct SetViewport {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
};

struct Use {
  Handle pipeline;
};

struct SetUniform {
  uint32_t arg_index;
  std::vector<uint8_t> value;
};

struct BindBuffer {
  Handle handle;
};

struct BindTexture {
  Handle handle;
};

struct UpdateBuffer {
  Handle buffer_handle;
  std::vector<uint8_t> data;
  size_t offset = 0;
};

struct BindVertexBuffer {
  std::optional<uint32_t> attr_idx;
  Handle handle;
};

struct BindIndexBuffer {
  Handle handle;
  size_t offset = 0;
};

struct Draw {
  uint32_t vertex_count;
  uint32_t instance_count = 1;
  uint32_t first_vertex = 0;
  uint32_t first_instance = 0;
};

struct DrawIndexed {
  uint32_t index_count;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

using Command = std::variant<SetViewport, BindBuffer, BindVertexBuffer,
                             BindIndexBuffer, BindTexture, UpdateBuffer,
                             Use, SetUniform, Draw, DrawIndexed>;

struct VertexAttribute {
  std::string name;
  uint32_t size;
  uint32_t location;
  uint32_t binding;
  uint64_t offset;
  uint32_t stride;
};

struct Uniform {
  std::string name;
  uint32_t binding;
  uint32_t size;
};

struct PipelineLayout {
  std::vector<VertexAttribute> attributes;
  std::vector<Uniform> uniforms;
};

enum class ShaderType {
  Vertex,
  Fragment,
  Compute,
};

struct Shader {
  ShaderType type;
  std::string source;
  std::string lang;
};

struct Pipeline {
  PipelineLayout layout;
  std::vector<Shader> shaders;
};

class Backend {
 public:
  virtual void interpret(std::span<const Command> commands) = 0;

  virtual Handle compilePipeline(Pipeline pipeline) = 0;

  virtual Handle upload(std::span<const uint8_t> buffer) = 0;

  virtual Handle allocDynamic(size_t size) = 0;
};
