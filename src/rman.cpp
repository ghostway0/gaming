#include <optional>
#include <string>
#include <unordered_map>

#include "sunset/property_tree.h"

#include "sunset/rman.h"

ResourceManager &ResourceManager::instance() {
  static ResourceManager rman{};
  return rman;
}

std::optional<PropertyTree> ResourceManager::getResource(std::string scope,
                                                         uint64_t id) {
  if (!resources_.contains(scope) || id >= resources_[scope].size()) {
    return std::nullopt;
  }

  return resources_[scope][id];
}

uint64_t ResourceManager::addResource(std::string scope,
                                      PropertyTree tree) {
  if (!resources_.contains(scope)) {
    resources_.try_emplace(scope, std::vector<PropertyTree>());
  }

  resources_[scope].push_back(tree);
  return resources_[scope].size() - 1;
}
