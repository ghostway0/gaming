#include "sunset/backend.h"
#include <iomanip>

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

std::ostream& operator<<(std::ostream& os, PrimitiveTopology pt) {
    switch (pt) {
        case PrimitiveTopology::Triangles: return os << "Triangles";
        case PrimitiveTopology::Lines: return os << "Lines";
        case PrimitiveTopology::Points: return os << "Points";
        default: return os << "UnknownTopology";
    }
}

std::ostream& operator<<(std::ostream& os, const SetViewport& v) {
    return os << "SetViewport{x=" << v.x << ", y=" << v.y
              << ", width=" << v.width << ", height=" << v.height << "}";
}

std::ostream& operator<<(std::ostream& os, const Use& u) {
    return os << "Use{pipeline=" << u.pipeline << "}";
}

std::ostream& operator<<(std::ostream& os, const SetUniform& s) {
    os << "SetUniform{arg_index=" << s.arg_index << ", value=[";
    for (size_t i = 0; i < s.value.size(); ++i) {
        if (i > 0) os << ", ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(s.value[i]);
    }
    return os << "]}";
}

std::ostream& operator<<(std::ostream& os, const BindBuffer& b) {
    return os << "BindBuffer{handle=" << b.handle << "}";
}

std::ostream& operator<<(std::ostream& os, const BindTexture& t) {
    return os << "BindTexture{handle=" << t.handle << "}";
}

std::ostream& operator<<(std::ostream& os, const UpdateBuffer& u) {
    os << "UpdateBuffer{buffer_handle=" << u.buffer_handle << ", offset=" << u.offset << ", data=[";
    for (size_t i = 0; i < u.data.size(); ++i) {
        if (i > 0) os << ", ";
        os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(u.data[i]);
    }
    return os << "]}";
}

std::ostream& operator<<(std::ostream& os, const BindVertexBuffer& b) {
    os << "BindVertexBuffer{attr_idx=";
    if (b.attr_idx.has_value())
        os << b.attr_idx.value();
    else
        os << "null";
    return os << ", handle=" << b.handle << "}";
}

std::ostream& operator<<(std::ostream& os, const BindIndexBuffer& b) {
    return os << "BindIndexBuffer{handle=" << b.handle << ", offset=" << b.offset << "}";
}

std::ostream& operator<<(std::ostream& os, const Draw& d) {
    return os << "Draw{vertex_count=" << d.vertex_count << ", instance_count=" << d.instance_count
              << ", first_vertex=" << d.first_vertex << ", first_instance=" << d.first_instance
              << ", primitive=" << d.primitive << "}";
}

std::ostream& operator<<(std::ostream& os, const DrawIndexed& d) {
    return os << "DrawIndexed{index_count=" << d.index_count
              << ", instance_count=" << d.instance_count
              << ", first_index=" << d.first_index
              << ", vertex_offset=" << d.vertex_offset
              << ", first_instance=" << d.first_instance
              << ", primitive=" << d.primitive << "}";
}

std::ostream& operator<<(std::ostream& os, const Command& cmd) {
    std::visit([&os](const auto& value) {
        os << value;
    }, cmd);
    return os;
}
