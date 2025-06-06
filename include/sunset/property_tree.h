#pragma once

#include <variant>
#include <string>
#include <vector>
#include <functional>

#include <absl/status/statusor.h>
#include <absl/log/log.h>

#include "sunset/utils.h"

using Property =
    std::variant<uint8_t, int16_t, int32_t, int64_t, float, double,
                 std::string, std::vector<uint8_t>, std::vector<int16_t>,
                 std::vector<int32_t>, std::vector<int64_t>,
                 std::vector<float>, std::vector<double>>;

struct PropertyTree {
  std::string name;
  std::vector<Property> properties;
  std::vector<PropertyTree> children;

  PropertyTree *getNodeByName(std::string_view name, size_t i = 0);
  PropertyTree const *getNodeByName(std::string_view name,
                                    size_t i = 0) const;
};

std::ostream &operator<<(std::ostream &os, const PropertyTree &tree);

using PropertyIterator = std::vector<Property>::const_iterator;

template <typename T>
struct FieldDescriptor {
  std::string_view name;
  std::function<absl::Status(T &, PropertyIterator &,
                             PropertyIterator const &,
                             const PropertyTree *context)>
      setter;
};

template <typename T>
struct TypeDeserializer {
  static std::vector<FieldDescriptor<T>> getFields() { return {}; }
};

template <typename T>
concept IsPropertyPrimitive = [] {
  return []<typename... Ts>(std::variant<Ts...> *) {
    return (std::disjunction_v<std::is_same<T, Ts>...>);
  }((Property *)nullptr);
}();

template <typename T>
absl::StatusOr<T> extractProperty(
    const Property &prop, std::string_view field_name = "",
    const PropertyTree *context_node = nullptr) {
  if constexpr (IsPropertyPrimitive<T>) {
    return std::visit(
        [&](const auto &value) -> absl::StatusOr<T> {
          using V = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<V, T>) {
            return value;
          } else {
            if (field_name.empty()) {
              return absl::InvalidArgumentError(absl::StrFormat(
                  "Property type mismatch: expected %s, got %s",
                  std::string(typeid(T).name()),
                  std::string(typeid(V).name())));
            } else {
              return absl::InvalidArgumentError(absl::StrFormat(
                  "Property type mismatch (%s): expected %s, got %s",
                  field_name, std::string(typeid(T).name()),
                  std::string(typeid(V).name())));
            }
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
      return absl::NotFoundError(
          absl::StrFormat("Child node '%s' not found", field_name));
    }
    return deserializeTree<T>(*child_node);
  }
}

// template <>
// struct TypeDeserializer<PropertyTree> {
//   static std::vector<FieldDescriptor<PropertyTree>> getFields() {
//     return {
//             FieldDescriptor<PropertyTree>[]
//     };
//   }
// };
//
template <typename T, typename FieldType>
FieldDescriptor<T> makeSetter(std::string_view name,
                              FieldType T::*field_ptr,
                              bool explicitly_named = false) {
  if constexpr (!IsPropertyPrimitive<FieldType>) {
    if constexpr (is_vector_v<FieldType>) {
      using V = std::decay_t<is_vector_t<FieldType>>;
      return FieldDescriptor<T>{
          name,
          std::function([field_ptr, name](
                            T &obj, PropertyIterator & /* prop_it */,
                            PropertyIterator const & /* end */,
                            const PropertyTree *context_node)
                            -> absl::Status {
            if (!context_node) {
              return absl::InternalError(
                  "Context node required for deserializing complex vector");
            }

            PropertyTree const *child_node =
                context_node->getNodeByName(name);
            if (!child_node) {
              return absl::NotFoundError(
                  absl::StrFormat("Child node '%s' not found", name));
            }

            (obj.*field_ptr).clear();
            (obj.*field_ptr).reserve(child_node->children.size());

            for (const PropertyTree &elem_tree : child_node->children) {
              V element = TRY(deserializeTree<V>(elem_tree));
              (obj.*field_ptr).push_back(element);
            }
            return absl::OkStatus();
          })};
    } else {
      return FieldDescriptor<T>{
          name, std::function(
                    [field_ptr, name](
                        T &obj, PropertyIterator & /* prop_it */,
                        PropertyIterator const & /* end */,
                        const PropertyTree *context_node) -> absl::Status {
                      PropertyTree const *child_node =
                          context_node->getNodeByName(name);
                      if (!child_node) {
                        return absl::NotFoundError(absl::StrFormat(
                            "Child node '%s' not found", name));
                      }

                      FieldType value =
                          TRY(deserializeTree<FieldType>(*child_node));

                      obj.*field_ptr = value;
                      return absl::OkStatus();
                    })};
    }
  } else {
    return FieldDescriptor<T>{
        name,
        std::function(
            [name, explicitly_named, field_ptr](
                T &obj, PropertyIterator &prop_it,
                PropertyIterator const &end,
                const PropertyTree *context_node) -> absl::Status {
              if (explicitly_named) {
                PropertyTree const *child_node =
                    context_node->getNodeByName(name);
                if (!child_node || child_node->properties.empty()) {
                  return absl::NotFoundError(absl::StrFormat(
                      "Primitive child node '%s' missing or empty", name));
                }

                obj.*field_ptr = TRY(extractProperty<FieldType>(
                    child_node->properties[0], name));
              } else {
                if (prop_it == end) {
                  return absl::InvalidArgumentError(absl::StrFormat(
                      "Property iterator is exhausted (%s)", name));
                }

                FieldType value =
                    TRY(extractProperty<FieldType>(*prop_it, name));
                ++prop_it;
                obj.*field_ptr = value;
              }

              return absl::OkStatus();
            })};
  }
}

template <typename T>
absl::StatusOr<T> deserializeTree(const PropertyTree &tree) {
  T result{};
  auto fields = TypeDeserializer<T>::getFields();
  auto prop_it = tree.properties.begin();

  for (const auto &field : fields) {
    auto status =
        field.setter(result, prop_it, tree.properties.end(), &tree);
    if (!status.ok()) {
      return status;
    }
  }

  return result;
}

template <>
inline absl::StatusOr<PropertyTree> deserializeTree(
    const PropertyTree &tree) {
  return tree;
}

absl::StatusOr<Property> readProperty(std::istream &input);

absl::StatusOr<PropertyTree> readPropertyTree(std::istream &input);
