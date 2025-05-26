#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sunset/property_tree.h"

class ResourceManager {
 public:
  static ResourceManager &instance();

  std::optional<PropertyTree> getResource(std::string scope, uint64_t id);
  
  uint64_t addResource(std::string scope, PropertyTree tree);

 private:
  std::unordered_map<std::string, std::vector<PropertyTree>> resources_;
};

struct RRef {
  std::string scope;
  int16_t resource_id;
};

template <>
struct TypeDeserializer<RRef> {
  static std::vector<FieldDescriptor<RRef>> getFields() {
    return {
        makeSetter("Scope", &RRef::scope, true),
        makeSetter("ResourceId", &RRef::resource_id, true),
    };
  }
};
