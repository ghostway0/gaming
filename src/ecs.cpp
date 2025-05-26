#include <absl/log/log.h>
#include <optional>

#include "sunset/ecs.h"

void Archetype::addEntity(Entity e) {
  entities.push_back(e);
  for (auto &[type, col] : columns) {
    LOG(INFO) << type.name();
    size_t comp_size =
        ComponentRegistry::instance().getTypeInfo(type).value().size;
    col.resize(entities.size() * comp_size, 0);
  }
}

void Archetype::removeEntity(size_t index) {
  if (index >= entities.size()) return;
  entities[index] = entities.back();
  entities.pop_back();

  for (auto &[type, col] : columns) {
    size_t comp_size =
        ComponentRegistry::instance().getTypeInfo(type).value().size;
    std::memmove(&col[index * comp_size], &col[entities.size() * comp_size],
                 comp_size);
    col.resize(entities.size() * comp_size);
  }
}

ComponentRegistry &ComponentRegistry::instance() {
  static ComponentRegistry r;
  return r;
}

std::optional<ComponentRegistry::SerializeFn>
ComponentRegistry::getSerializer(std::string t) const {
  if (!serializers_.contains(t)) {
    return std::nullopt;
  }
  return serializers_.at(t);
}

std::optional<ComponentRegistry::DeserializeFn>
ComponentRegistry::getDeserializer(std::string t) const {
  if (!deserializers_.contains(t)) {
    return std::nullopt;
  }
  return deserializers_.at(t);
}

std::optional<ComponentType> ComponentRegistry::getTypeInfo(
    std::type_index t) const {
  if (auto it = types_.find(t); it != types_.end()) {
    return it->second;
  }
  return std::nullopt;
}

ECS::ECS() : next_entity_(1), free_entities_(0) {}

Entity ECS::createEntity() {
  if (!free_entities_.empty()) {
    Entity e = free_entities_.back();
    free_entities_.pop_back();
    return e;
  }
  Entity e = next_entity_++;
  if (e >= entity_locations_.size()) {
    entity_locations_.resize(e + 1, {nullptr, 0});
  }
  return e;
}

void ECS::destroyEntity(Entity e) {
  auto &loc = entity_locations_[e];
  if (loc.first) {
    loc.first->removeEntity(loc.second);
    loc = {nullptr, 0};
    free_entities_.push_back(e);
  }
}

Archetype &ECS::getOrCreateArchetype(const ComponentSignature &sig) {
  auto it = archetypes_.find(sig);
  if (it != archetypes_.end()) return it->second;
  Archetype a;
  a.signature = sig;
  for (const auto &type : sig) {
    a.columns[type] = {};
  }
  return archetypes_[sig] = std::move(a);
}

bool ECS::containsSignature(const ComponentSignature &arch_sig,
                            const ComponentSignature &query_sig) const {
  return std::includes(arch_sig.begin(), arch_sig.end(), query_sig.begin(),
                       query_sig.end());
}

void ECS::addComponentRaw(Entity e, Any data) {
  auto [old_arch, old_index] = entity_locations_[e];

  ComponentSignature new_sig{};
  if (old_arch) {
    new_sig = old_arch->signature;
  }
  new_sig.insert(data.type());

  Archetype &new_arch = getOrCreateArchetype(new_sig);
  size_t new_index = new_arch.entities.size();
  new_arch.addEntity(e);

  if (old_arch) {
    copyComponents(old_arch, old_index, new_arch, new_index,
                   old_arch->signature);
    old_arch->removeEntity(old_index);
  }

  new_arch.addComponentRaw(new_index, std::move(data));

  entity_locations_[e] = {&new_arch, new_index};
}
