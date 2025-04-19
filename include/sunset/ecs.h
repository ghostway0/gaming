#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <functional>
#include <iostream>
#include <memory>
#include <algorithm>

#include <absl/status/status.h>

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
struct hash<std::vector<std::type_index>> {
  size_t operator()(const std::vector<std::type_index> &sig) const {
    size_t seed = 0;
    for (const auto &type : sig) {
      seed ^= std::hash<std::type_index>{}(type) + 0x9e3779b9 +
              (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

} // namespace std

using ComponentSignature = std::vector<std::type_index>;

inline ComponentSignature make_signature(
    std::initializer_list<std::type_index> types) {
  ComponentSignature sig(types);
  std::sort(sig.begin(), sig.end());
  return sig;
}

struct Archetype {
  ComponentSignature signature;
  std::vector<Entity> entities;
  std::unordered_map<std::type_index, std::vector<uint8_t>> columns;
  std::unordered_map<std::type_index, size_t> component_sizes;

  void addEntity(Entity e);

  template <typename T>
  void addComponent(size_t index, const T &comp) {
    auto type = std::type_index(typeid(T));
    auto &col = columns[type];
    std::memcpy(&col[index * sizeof(T)], &comp, sizeof(T));
  }

  void removeEntity(size_t index);

  template <typename T>
  T *getComponent(size_t index) {
    auto type = std::type_index(typeid(T));
    auto &col = columns.at(type);
    return reinterpret_cast<T *>(&col[index * sizeof(T)]);
  }
};

class ComponentRegistry {
 public:
  using SerializeFn = std::function<void(std::ostream &, const void *)>;
  using DeserializeFn =
      std::function<std::unique_ptr<void, void (*)(void *)>(
          std::istream &)>;

  static ComponentRegistry &instance();

  template <typename T>
  void registerType() {
    auto type = std::type_index(typeid(T));
    types_.try_emplace(type, ComponentType{type, sizeof(T)});
    serializers_[type] = [](std::ostream &os, const void *ptr) {
      const auto *comp = reinterpret_cast<const T *>(ptr);
      comp->serialize(os);
    };
    deserializers_[type] =
        [](std::istream &is) -> std::unique_ptr<void, void (*)(void *)> {
      T *raw = new T(T::deserialize(is));
      return {static_cast<void *>(raw),
              [](void *ptr) { delete static_cast<T *>(ptr); }};
    };
  }

  SerializeFn getSerializer(std::type_index t) const;

  DeserializeFn getDeserializer(std::type_index t) const;

  absl::optional<ComponentType> getTypeInfo(std::type_index t) const;

 private:
  std::unordered_map<std::type_index, ComponentType> types_;
  std::unordered_map<std::type_index, SerializeFn> serializers_;
  std::unordered_map<std::type_index, DeserializeFn> deserializers_;
};

class ECS {
 public:
  ECS();

  Entity createEntity();

  template <typename... Cs>
  absl::Status addComponents(Entity e, const Cs &...comps) {
    ComponentSignature sig =
        make_signature({std::type_index(typeid(Cs))...});

    (ComponentRegistry::instance().registerType<Cs>(), ...);

    auto &arch = getOrCreateArchetype(sig);
    size_t index = arch.entities.size();

    arch.addEntity(e);
    entity_locations_[e] = {&arch, index};
    (arch.addComponent(index, comps), ...);

    return absl::OkStatus();
  }

  template <typename T>
  void setComponent(Entity e, const T &comp) {
    auto *arch = entity_locations_[e].first;
    if (!arch) return;
    size_t index = entity_locations_[e].second;
    arch->addComponent(index, comp);
  }

  void destroyEntity(Entity e);

  template <typename... Cs>
  void forEach(std::function<void(Entity, Cs *...)> callback) {
    ComponentSignature query =
        make_signature({std::type_index(typeid(Cs))...});
    for (auto &[sig, arch] : archetypes_) {
      if (containsSignature(sig, query)) {
        for (size_t i = 0; i < arch.entities.size(); ++i) {
          callback(arch.entities[i], arch.getComponent<Cs>(i)...);
        }
      }
    }
  }

 private:
  Entity next_entity_;
  std::vector<Entity> free_entities_;
  std::unordered_map<ComponentSignature, Archetype> archetypes_;
  std::vector<std::pair<Archetype *, size_t>> entity_locations_;

  Archetype &getOrCreateArchetype(const ComponentSignature &sig);
  bool validateSignature(const ComponentSignature &sig);
  bool containsSignature(const ComponentSignature &arch_sig,
                         const ComponentSignature &query_sig) const;
};
