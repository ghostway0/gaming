#include <absl/log/log.h>
#include <optional>
#include <utility>

#include "sunset/ecs.h"

void Archetype::addEntity(Entity e) {
  entities.push_back(e);
  for (auto &[type, col] : columns) {
    size_t comp_size =
        ComponentRegistry::instance().getTypeInfo(type).value().size;
    col.resize(entities.size() * comp_size, 0);
  }
}

std::optional<EntitySwap> Archetype::removeEntity(size_t index) {
  if (index >= entities.size()) {
    return std::nullopt;
  }

  if (index == entities.size() - 1) {
    entities.pop_back();
    return std::nullopt;
  }

  entities[index] = entities.back();
  entities.pop_back();

  for (auto &[type, col] : columns) {
    size_t comp_size =
        ComponentRegistry::instance().getTypeInfo(type).value().size;
    std::memmove(&col[index * comp_size], &col[entities.size() * comp_size],
                 comp_size);
    col.resize(entities.size() * comp_size);
  }

  return EntitySwap{entities[index], index};
}

void Archetype::addComponentRaw(size_t index, Any data) {
  std::vector<uint8_t> &col = columns[data.type()];
  size_t size =
      ComponentRegistry::instance().getTypeInfo(data.type()).value().size;

  if (col.size() < (index + 1) * size) {
    col.resize((index + 1) * size);
  }

  std::memcpy(&col[index * size], data.get(), size);
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

ECS::ECS() : next_entity_(1), free_entities_() {}

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
    removeEntityImpl(e);
    loc = {nullptr, 0};
    free_entities_.push_back(e);
  }
}

Archetype *ECS::getOrCreateArchetype(const ComponentSignature &sig) {
  if (archetypes_.contains(sig)) {
    return archetypes_[sig];
  }

  Archetype *a = new Archetype();
  a->signature = sig;
  for (const auto &type : sig) {
    a->columns[type] = {};
  }
  archetypes_[sig] = a;
  return archetypes_[sig];
}

bool ECS::containsSignature(const ComponentSignature &arch_sig,
                            const ComponentSignature &query_sig) const {
  return std::includes(arch_sig.begin(), arch_sig.end(), query_sig.begin(),
                       query_sig.end());
}

void ECS::addComponentRaw(Entity e, Any data) {
  auto [old_arch, old_index] = entity_locations_[e];

  ComponentSignature new_sig =
      (old_arch) ? old_arch->signature : ComponentSignature{};
  new_sig.insert(data.type());

  Archetype *new_arch = getOrCreateArchetype(new_sig);
  size_t new_index = new_arch->entities.size();
  new_arch->addEntity(e);

  if (old_arch) {
    copyComponents(old_arch, old_index, new_arch, new_index,
                   old_arch->signature);
    removeEntityImpl(e);
  }

  new_arch->addComponentRaw(new_index, std::move(data));

  entity_locations_[e] = std::make_pair(new_arch, new_index);
}

void ECS::removeEntityImpl(Entity e) {
  auto [arch, index] = entity_locations_[e];
  if (!arch) {
    return;
  }

  std::optional<EntitySwap> swap = arch->removeEntity(index);
  if (swap.has_value()) {
    entity_locations_[swap->entity] = std::make_pair(arch, swap->index);
  }
}
