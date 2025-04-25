#pragma once

#include <variant>
#include <string>
#include <vector>
#include <functional>

#include <absl/status/statusor.h>
#include <absl/log/log.h>

using Property =
    std::variant<int64_t, float, double, std::string, std::vector<uint8_t>>;

struct PropertyTree {
  std::string name;
  std::vector<Property> properties;
  std::vector<PropertyTree> children;

  PropertyTree *getNodeByName(std::string_view name);
  PropertyTree const *getNodeByName(std::string_view name) const;
};

std::ostream &operator<<(std::ostream &os, const PropertyTree &tree);

template <typename T>
struct FieldDescriptor {
  std::string_view name;
  std::function<absl::Status(T &, const Property &,
                             const PropertyTree *context)>
      setter;
};

template <typename T>
struct TypeDeserializer {
  static std::vector<FieldDescriptor<T>> getFields() { return {}; }
};

template <typename T>
concept Deserializable = [] {
  return []<typename... Ts>(std::variant<Ts...> *) {
    return (std::disjunction_v<std::is_same<T, Ts>...>);
  }((Property *)nullptr);
}();

template <typename T>
absl::StatusOr<T> extractProperty(
    const Property &prop, std::string_view field_name = "",
    const PropertyTree *context_node = nullptr) {
  if constexpr (Deserializable<T>) {
    return std::visit(
        [](const auto &value) -> absl::StatusOr<T> {
          using V = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<V, T>) {
            return value;
          } else {
            return absl::InvalidArgumentError(
                "Property type mismatch: expected " +
                std::string(typeid(T).name()) + ", got " +
                std::string(typeid(V).name()));
          }
        },
        prop);
  } else {
    if (!context_node) {
      return absl::InvalidArgumentError(
          "Context node required for deserializing complex type " +
          std::string(typeid(T).name()));
    }
    auto *child_node = context_node->getNodeByName(field_name);
    if (!child_node) {
      return absl::NotFoundError("Child node " + std::string(field_name) +
                                 " not found");
    }
    return deserializeNode<T>(*child_node);
  }
}

template <typename T, typename FieldType>
FieldDescriptor<T> makeSetter(std::string_view name,
                              FieldType T::*field_ptr) {
  if constexpr (!Deserializable<FieldType>) {
    return FieldDescriptor<T>{
        name,
        std::function([field_ptr, name](T &obj, const Property & /* prop */,
                                        const PropertyTree *context_node)
                          -> absl::Status {
          if (!context_node) {
            return absl::InternalError(
                "Context node required for deserializing complex type");
          }

          auto *child_node = context_node->getNodeByName(name);
          if (!child_node) {
            return absl::NotFoundError(
                absl::StrFormat("Child node %s not found", name));
          }

          auto value = deserializeNode<FieldType>(*child_node);
          if (!value.ok()) {
            return value.status();
          }

          obj.*field_ptr = *value;
          return absl::OkStatus();
        })};
  } else {
    return FieldDescriptor<T>{
        name,
        std::function([field_ptr](T &obj, const Property &prop,
                                  const PropertyTree * /* context_node */)
                          -> absl::Status {
          auto value = extractProperty<FieldType>(prop);
          if (!value.ok()) {
            return value.status();
          }
          obj.*field_ptr = *value;
          return absl::OkStatus();
        })};
  }
}

template <typename T>
absl::StatusOr<T> deserializeNode(const PropertyTree &tree) {
  T result{};
  auto fields = TypeDeserializer<T>::getFields();
  size_t prop_index = 0;

  // should we use a property iterator instead of giving it one?
  // it can ++ it if it uses anything there

  for (const auto &field : fields) {
    Property prop = (prop_index < tree.properties.size())
                        ? tree.properties[prop_index]
                        : Property{};
    auto status = field.setter(result, prop, &tree);
    if (!status.ok()) {
      return status;
    }

    // this is wrong it should be Deserializable<FieldType>
    if constexpr (Deserializable<T>) {
      ++prop_index;
    }
  }

  return result;
}

absl::StatusOr<Property> readProperty(std::istream &input);

std::optional<PropertyTree> readPropertyTree(std::istream &input);
