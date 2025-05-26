#pragma once

#include <cstddef>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <functional>
#include "sunset/property_tree.h"
#include "sunset/utils.h"

#include <absl/status/status.h>
#include <absl/status/statusor.h>

using Entity = uint32_t;

struct ComponentType {
  std::type_index type;
  size_t size;

  bool operator==(const ComponentType &other) const {
    return type == other.type;
  }
};

namespace std {

template <>
struct hash<std::set<std::type_index>> {
  size_t operator()(const std::set<std::type_index> &sig) const {
    size_t seed = 0;
    for (const auto &type : sig) {
      seed ^= std::hash<std::type_index>{}(type) + 0x9e3779b9 +
              (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

} // namespace std

using ComponentSignature = std::set<std::type_index>;

class Any {
 public:
  template <typename T>
  explicit Any(T &&value) : ptr_(new T(value)), type_index_{typeid(T)} {}

  void *get() { return ptr_; }

  std::type_index type() { return type_index_; }

 private:
  void *ptr_;
  std::type_index type_index_;
};

class ComponentRegistry {
 public:
  using SerializeFn = std::function<std::optional<PropertyTree>(Any any)>;
  using DeserializeFn =
      std::function<absl::StatusOr<Any>(PropertyTree const &tree)>;

  static ComponentRegistry &instance();

  template <typename T>
  void registerType() {
    auto type = std::type_index(typeid(T));
    std::string type_id = demangle(typeid(T).name());
    types_.try_emplace(type, ComponentType{type, sizeof(T)});

    serializers_[type_id] = [](Any any) -> std::optional<PropertyTree> {
      T const *comp = reinterpret_cast<T const *>(any.get());
      return comp->serialize();
    };
    deserializers_[type_id] =
        [](PropertyTree const &tree) -> absl::StatusOr<Any> {
      T r = TRY(T::deserialize(tree));
      return Any(std::move(r));
    };
  }

  std::optional<SerializeFn> getSerializer(std::string t) const;

  std::optional<DeserializeFn> getDeserializer(std::string t) const;

  absl::optional<ComponentType> getTypeInfo(std::type_index t) const;

 private:
  std::unordered_map<std::type_index, ComponentType> types_;
  std::unordered_map<std::string, SerializeFn> serializers_;
  std::unordered_map<std::string, DeserializeFn> deserializers_;
};

class ECS;

struct Archetype {
  ComponentSignature signature;
  std::vector<Entity> entities;
  std::unordered_map<std::type_index, std::vector<uint8_t>> columns;

  void addEntity(Entity e);

  void addComponentRaw(size_t index, Any data);

  template <typename T>
  void addComponent(size_t index, const T &comp) {
    auto type = std::type_index(typeid(T));
    auto &col = columns[type];

    if (col.size() < (index + 1) * sizeof(T)) {
      col.resize((index + 1) * sizeof(T), 0);
    }

    std::memcpy(&col[index * sizeof(T)], &comp, sizeof(T));
  }

  void removeEntity(size_t index, ECS &ecs);

  template <typename T>
  T *getComponent(size_t index) {
    auto type = std::type_index(typeid(T));
    auto it = columns.find(type);
    if (it == columns.end()) {
      return nullptr;
    }
    return reinterpret_cast<T *>(&it->second[index * sizeof(T)]);
  }
};

class ECS {
 public:
  ECS();

  Entity createEntity();

  template <typename... Cs>
  void addComponents(Entity e, const Cs &...comps) {
    ComponentSignature new_comps_sig({std::type_index(typeid(Cs))...});

    (ComponentRegistry::instance().registerType<Cs>(), ...);

    auto [old_arch, old_index] = entity_locations_[e];

    ComponentSignature old_sig = {};
    if (old_arch) {
      old_sig = old_arch->signature;
    }

    ComponentSignature new_sig = old_sig;
    for (const auto &type : new_comps_sig) {
      new_sig.insert(type);
    }

    Archetype &new_arch = getOrCreateArchetype(new_sig);
    size_t new_index = new_arch.entities.size();
    new_arch.addEntity(e);

    if (old_arch) {
      copyComponents(old_arch, old_index, &new_arch, new_index, old_sig);
      old_arch->removeEntity(old_index, *this);
    }

    (new_arch.addComponent(new_index, comps), ...);

    entity_locations_[e] = std::make_pair(&new_arch, new_index);
  }

  void addComponentRaw(Entity e, Any data);

  template <typename C>
  void removeComponent(Entity e) {
    auto [old_arch, old_index] = entity_locations_[e];
    if (!old_arch) {
      return;
    }

    auto old_sig = old_arch->signature;
    ComponentSignature new_sig = old_sig;
    new_sig.erase(std::type_index(typeid(C)));

    Archetype &new_arch = getOrCreateArchetype(new_sig);
    size_t new_index = new_arch.entities.size();
    new_arch.addEntity(e);

    copyComponents(old_arch, old_index, &new_arch, new_index, new_sig);
    old_arch->removeEntity(old_index, *this);

    entity_locations_[e] = std::make_pair(&new_arch, new_index);
  }

  template <typename T>
  void setComponent(Entity e, const T &comp) {
    auto *arch = entity_locations_[e].first;
    if (!arch) return;
    size_t index = entity_locations_[e].second;
    arch->addComponent(index, comp);
  }

  template <typename T>
  T const *getComponent(Entity e) const {
    auto [archetype, index] = entity_locations_[e];
    return archetype->getComponent<T>(index);
  }

  template <typename T>
  T *getComponent(Entity e) {
    auto [archetype, index] = entity_locations_[e];
    return archetype->getComponent<T>(index);
  }

  void destroyEntity(Entity e);

  template <typename... Cs>
  void forEach(std::function<void(Entity, Cs *...)> callback) {
    ComponentSignature query =
        ComponentSignature({std::type_index(typeid(Cs))...});
    for (auto &[sig, arch] : archetypes_) {
      if (containsSignature(sig, query)) {
        for (size_t i = 0; i < arch.entities.size(); i++) {
          callback(arch.entities[i], arch.getComponent<Cs>(i)...);
        }
      }
    }
  }

 private:
  void copyComponents(Archetype *old_arch, size_t old_index,
                      Archetype *new_arch, size_t new_index,
                      const ComponentSignature &copy_sig) {
    for (const auto &type : copy_sig) {
      auto it_src_col = old_arch->columns.find(type);
      size_t comp_size =
          ComponentRegistry::instance().getTypeInfo(type)->size;

      const auto &src_col = it_src_col->second;
      new_arch->columns[type].resize((new_index + 1) * comp_size, 0);
      std::memcpy(&new_arch->columns[type][new_index * comp_size],
                  &src_col[old_index * comp_size], comp_size);
    }
  }

  Entity next_entity_;
  std::vector<Entity> free_entities_;
  std::unordered_map<ComponentSignature, Archetype> archetypes_;
  std::vector<std::pair<Archetype *, size_t>> entity_locations_;

  friend struct Archetype;

  Archetype &getOrCreateArchetype(const ComponentSignature &sig);
  bool containsSignature(const ComponentSignature &arch_sig,
                         const ComponentSignature &query_sig) const;
};
