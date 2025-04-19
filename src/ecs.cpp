#include <absl/log/log.h>

#include "sunset/ecs.h"

void Archetype::addEntity(Entity e) {
  entities.push_back(e);
  for (auto &[type, col] : columns) {
    col.resize(entities.size() * component_sizes[type], 0);
  }
}

void Archetype::removeEntity(size_t index) {
  if (index >= entities.size()) return;
  entities[index] = entities.back();
  entities.pop_back();

  for (auto &[type, col] : columns) {
    size_t comp_size = component_sizes[type];
    std::memmove(&col[index * comp_size], &col[entities.size() * comp_size],
                 comp_size);
    col.resize(entities.size() * comp_size);
  }
}

ComponentRegistry &ComponentRegistry::instance() {
  static ComponentRegistry r;
  return r;
}

ComponentRegistry::SerializeFn ComponentRegistry::getSerializer(
    std::type_index t) const {
  return serializers_.at(t);
}

ComponentRegistry::DeserializeFn ComponentRegistry::getDeserializer(
    std::type_index t) const {
  return deserializers_.at(t);
}

absl::optional<ComponentType> ComponentRegistry::getTypeInfo(
    std::type_index t) const {
  if (auto it = types_.find(t); it != types_.end()) {
    return it->second;
  }
  return absl::nullopt;
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
    auto info = ComponentRegistry::instance().getTypeInfo(type);
    a.columns[type] = {};
    a.component_sizes[type] = info->size;
  }
  return archetypes_[sig] = std::move(a);
}

bool ECS::validateSignature(const ComponentSignature &sig) {
  for (const auto &t : sig) {
    if (!ComponentRegistry::instance().getTypeInfo(t)->type.name()) {
      return false;
    }
  }
  return true;
}

bool ECS::containsSignature(const ComponentSignature &arch_sig,
                            const ComponentSignature &query_sig) const {
  if (query_sig.size() > arch_sig.size()) return false;
  size_t i = 0, j = 0;
  while (i < query_sig.size() && j < arch_sig.size()) {
    if (query_sig[i] == arch_sig[j]) ++i;
    ++j;
  }
  return i == query_sig.size();
}
