#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <optional>
#include <variant>
#include <span>
#include "sunset/image.h"

using Handle = uint64_t;

enum class PrimitiveTopology {
  Triangles,
  Lines,
  Points,
};

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
  PrimitiveTopology primitive = PrimitiveTopology::Triangles;
};

struct DrawIndexed {
  uint32_t index_count;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
  PrimitiveTopology primitive = PrimitiveTopology::Triangles;
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

using RawEmitFn =
    std::function<void(std::vector<Command> &, const void *args)>;

struct Pipeline {
  Handle handle;
  RawEmitFn emit;

  template <typename... Args>
  void operator()(std::vector<Command> &commands, Args &&...args) {
    auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
    commands.push_back(Use{handle});
    emit(commands, &tuple_args);
  }
};

class Backend {
 public:
  virtual void interpret(std::span<const Command> commands) = 0;

  virtual Handle compilePipeline(PipelineLayout layout,
                                 std::vector<Shader> shaders) = 0;

  virtual Handle upload(std::span<const uint8_t> buffer) = 0;

  virtual Handle allocDynamic(size_t size) = 0;

  virtual Handle uploadTexture(Image const &image) = 0;
};

class PipelineBuilder {
 public:
  PipelineBuilder(Backend &backend);

  Pipeline build();

  PipelineBuilder &vertexAttr(VertexAttribute attr);

  PipelineBuilder &uniform(Uniform uniform);

  PipelineBuilder &shader(Shader shader);

  template <typename... Args>
  PipelineBuilder &emitFn(
      std::function<void(std::vector<Command> &, Args...)> emit_fn) {
    emit_fn_ = [emit_fn](std::vector<Command> &cmds, const void *args_raw) {
      const auto &args =
          *reinterpret_cast<const std::tuple<Args...> *>(args_raw);
      std::apply([&](Args... unpacked) { emit_fn(cmds, unpacked...); },
                 args);
    };
    return *this;
  }

 private:
  Backend &backend_;
  std::vector<VertexAttribute> vertex_attrs_;
  std::vector<Uniform> uniforms_;
  std::vector<Shader> shaders_;
  RawEmitFn emit_fn_;
};
