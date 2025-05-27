#include <iomanip>

#include "sunset/ecs.h"
#include "sunset/rman.h"
#include "sunset/geometry.h"

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

std::ostream &operator<<(std::ostream &os, PrimitiveTopology pt) {
  switch (pt) {
    case PrimitiveTopology::Triangles:
      return os << "Triangles";
    case PrimitiveTopology::Lines:
      return os << "Lines";
    case PrimitiveTopology::Points:
      return os << "Points";
    default:
      return os << "UnknownTopology";
  }
}

std::ostream &operator<<(std::ostream &os, const SetViewport &v) {
  return os << "SetViewport{x=" << v.x << ", y=" << v.y
            << ", width=" << v.width << ", height=" << v.height << "}";
}

std::ostream &operator<<(std::ostream &os, const Use &u) {
  return os << "Use{pipeline=" << u.pipeline << "}";
}

std::ostream &operator<<(std::ostream &os, const SetUniform &s) {
  os << "SetUniform{arg_index=" << s.arg_index << ", value=[";
  for (size_t i = 0; i < s.value.size(); ++i) {
    if (i > 0) os << ", ";
    os << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(s.value[i]);
  }
  return os << "]}";
}

std::ostream &operator<<(std::ostream &os, const BindBuffer &b) {
  return os << "BindBuffer{handle=" << b.handle << "}";
}

std::ostream &operator<<(std::ostream &os, const BindTexture &t) {
  return os << "BindTexture{handle=" << t.handle << "}";
}

std::ostream &operator<<(std::ostream &os, const UpdateBuffer &u) {
  os << "UpdateBuffer{buffer_handle=" << u.buffer_handle
     << ", offset=" << u.offset << ", data=[";
  for (size_t i = 0; i < u.data.size(); ++i) {
    if (i > 0) os << ", ";
    os << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(u.data[i]);
  }
  return os << "]}";
}

std::ostream &operator<<(std::ostream &os, const BindVertexBuffer &b) {
  os << "BindVertexBuffer{attr_idx=";
  if (b.attr_idx.has_value())
    os << b.attr_idx.value();
  else
    os << "null";
  return os << ", handle=" << b.handle << "}";
}

std::ostream &operator<<(std::ostream &os, const BindIndexBuffer &b) {
  return os << "BindIndexBuffer{handle=" << b.handle
            << ", offset=" << b.offset << "}";
}

std::ostream &operator<<(std::ostream &os, const Draw &d) {
  return os << "Draw{vertex_count=" << d.vertex_count
            << ", instance_count=" << d.instance_count
            << ", first_vertex=" << d.first_vertex
            << ", first_instance=" << d.first_instance
            << ", primitive=" << d.primitive << "}";
}

std::ostream &operator<<(std::ostream &os, const DrawIndexed &d) {
  return os << "DrawIndexed{index_count=" << d.index_count
            << ", instance_count=" << d.instance_count
            << ", first_index=" << d.first_index
            << ", vertex_offset=" << d.vertex_offset
            << ", first_instance=" << d.first_instance
            << ", primitive=" << d.primitive << "}";
}

std::ostream &operator<<(std::ostream &os, const Command &cmd) {
  std::visit([&os](const auto &value) { os << value; }, cmd);
  return os;
}

class UploadCache {
 public:
  static UploadCache &instance() {
    static UploadCache cache{};
    return cache;
  }

  std::optional<Handle> get(const RRef &rref) {
    auto it = cache_.find(rref);
    if (it == cache_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void insert(const RRef &rref, Handle handle) {
    cache_.emplace(rref, handle);
  }

  void insert(const RRef &rref, Handle &&handle) {
    cache_.emplace(rref, std::move(handle));
  }

  bool contains(const RRef &rref) const {
    return cache_.find(rref) != cache_.end();
  }

 private:
  std::unordered_map<RRef, Handle> cache_;
};

void compileScene(ECS &ecs, Backend &backend) {
  std::vector<std::pair<Entity, MeshRenderable>> to_add;

  ecs.forEach(std::function([&](Entity entity, MeshRef *mesh_ref) {
    PropertyTree tree =
        ResourceManager::instance()
            .getResource(mesh_ref->rref.scope, mesh_ref->rref.resource_id)
            .value();

    absl::StatusOr<SavedMesh> saved_mesh = deserializeTree<SavedMesh>(tree);
    if (!saved_mesh.ok()) return;

    std::optional<Handle> texture_handle = std::nullopt;
    std::optional<std::string> texture_path = std::nullopt;

    // Handle texture processing with cache
    if (TextureRef *texture_ref = ecs.getComponent<TextureRef>(entity)) {
      // Check if texture is already cached
      if (UploadCache::instance().contains(texture_ref->rref)) {
        texture_handle = UploadCache::instance().get(texture_ref->rref);
      } else {
        // Texture not cached, need to load and upload it
        std::optional<PropertyTree> tex_tree =
            ResourceManager::instance()
                .getResource(texture_ref->rref.scope,
                             texture_ref->rref.resource_id)
                .value();

        absl::StatusOr<Texture> saved =
            deserializeTree<Texture>(tex_tree.value());
        assert(saved.ok());
        texture_path = saved->src;

        std::optional<Image> texture_image;
        if (texture_path.has_value()) {
          absl::StatusOr<Image> result = loadTextureFromSrc(*texture_path);
          if (result.ok()) {
            texture_image = *result;
            // Upload texture to backend and cache the handle
            Handle uploaded_texture = backend.uploadTexture(*texture_image);
            UploadCache::instance().insert(texture_ref->rref, uploaded_texture);
            texture_handle = uploaded_texture;
          }
        }
      }
    }

    // Handle mesh processing with cache
    std::optional<Handle> mesh_vertex_handle = std::nullopt;
    std::optional<Handle> mesh_index_handle = std::nullopt;
    
    // Check if mesh is already cached
    if (UploadCache::instance().contains(mesh_ref->rref)) {
      // For mesh, we might need to store both vertex and index handles
      // This assumes the cached Handle represents the vertex buffer
      // You might need to modify this based on your actual mesh caching strategy
      mesh_vertex_handle = UploadCache::instance().get(mesh_ref->rref);
      
      // If you need separate caching for vertex and index buffers, you could use
      // modified RRefs or store a compound handle structure
    }
    
    absl::StatusOr<MeshRenderable> renderable;
    
    if (mesh_vertex_handle.has_value()) {
      // Mesh is cached, construct MeshRenderable from cached data
      // You'll need to also cache other mesh data like vertex_count, index_count, normal
      // This is a simplified version - you might need to extend your cache to store
      // complete MeshRenderable data or use a different caching strategy
      
      // For now, we still need to load the mesh to get counts and normal
      // A more sophisticated approach would cache the entire MeshRenderable
      std::optional<Image> texture_image; // We already have the handle, so no image needed
      renderable = loadSavedMesh(*mesh_ref, *saved_mesh, texture_image, backend);
      
      if (renderable.ok()) {
        // Replace the uploaded handles with cached ones
        renderable->vertex_buffer = *mesh_vertex_handle;
        if (texture_handle.has_value()) {
          renderable->texture = texture_handle;
        }
      }
    } else {
      // Mesh not cached, need to load and upload it
      std::optional<Image> texture_image; // We handle texture separately now
      renderable = loadSavedMesh(*mesh_ref, *saved_mesh, texture_image, backend);
      
      if (renderable.ok()) {
        // Cache the mesh handles
        UploadCache::instance().insert(mesh_ref->rref, renderable->vertex_buffer);
        
        // Set the texture handle if we have one
        if (texture_handle.has_value()) {
          renderable->texture = texture_handle;
        }
      }
    }

    if (renderable.ok()) {
      to_add.emplace_back(entity, std::move(*renderable));
    }
  }));

  for (auto &[entity, renderable] : to_add) {
    ecs.addComponents(entity, renderable);
    ecs.removeComponent<MeshRef>(entity);
    ecs.removeComponent<TextureRef>(entity);
  }
}
