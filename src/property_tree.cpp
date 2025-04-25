#include <bit>
#include <istream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>
#include <absl/status/statusor.h>
#include <zlib.h>

#include "sunset/property_tree.h"

namespace {

template <typename T>
T readValue(std::istream &input) {
  T value;
  input.read(std::bit_cast<char *>(&value), sizeof(T));
  return value;
}

absl::StatusOr<std::vector<uint8_t>> decompressData(
    const std::vector<uint8_t> &compressed_data,
    uint32_t uncompressed_size) {
  std::vector<uint8_t> decompressed_data(uncompressed_size);
  z_stream stream = {};

  if (inflateInit(&stream) != Z_OK) {
    return absl::InternalError("Failed to initialize decompression stream");
  }

  stream.avail_in = compressed_data.size();
  stream.next_in = const_cast<Bytef *>(compressed_data.data());
  stream.avail_out = decompressed_data.size();
  stream.next_out = decompressed_data.data();

  int ret = inflate(&stream, Z_FINISH);
  inflateEnd(&stream);

  if (ret != Z_STREAM_END) {
    return absl::InternalError("Failed to decompress data");
  }

  return decompressed_data;
}

template <typename T>
absl::StatusOr<T> extractProperty(
    const Property &prop, std::string_view field_name = "",
    const PropertyTree *context_node = nullptr) {
  if constexpr (Deserializable<T>) {
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
  } else {
    return extractProperty<T>(prop);
  }
}

} // namespace

PropertyTree *PropertyTree::getNodeByName(std::string_view name) {
  PropertyTree *current = this;

  for (const std::string_view &sub : absl::StrSplit(name, ".")) {
    bool found = false;
    for (PropertyTree &child : current->children) {
      if (child.name == sub) {
        current = &child;
        found = true;
        break;
      }
    }

    if (!found) {
      return nullptr;
    }
  }

  return current;
}

PropertyTree const *PropertyTree::getNodeByName(
    std::string_view name) const {
  return const_cast<PropertyTree *>(this)->getNodeByName(name);
}

absl::StatusOr<Property> readProperty(std::istream &input) {
  char type = readValue<char>(input);
  switch (type) {
    case 'Y':
      return static_cast<int64_t>(readValue<int16_t>(input));
    case 'C':
      return static_cast<int64_t>(readValue<uint8_t>(input));
    case 'I':
      return static_cast<int64_t>(readValue<int32_t>(input));
    case 'F':
      return static_cast<double>(readValue<float>(input));
    case 'D':
      return readValue<double>(input);
    case 'L':
      return readValue<int64_t>(input);
    case 'S':
    case 'R': {
      uint32_t len = readValue<uint32_t>(input);
      std::string data(len, '\0');
      input.read(data.data(), len);
      return data;
    }
    case 'f':
    case 'd':
    case 'l':
    case 'i':
    case 'b': {
      uint32_t array_len = readValue<uint32_t>(input);
      uint32_t encoding = readValue<uint32_t>(input);
      uint32_t compressed_len = readValue<uint32_t>(input);
      std::vector<uint8_t> data(compressed_len);
      input.read(reinterpret_cast<char *>(data.data()), compressed_len);
      if (encoding == 1) {
        return decompressData(data, array_len);
      } else {
        return absl::UnimplementedError(
            absl::StrFormat("Encoding %d not yet implemented", encoding));
      }
      return data;
    }
    default:
      return absl::InvalidArgumentError("Invalid type info");
  }
}

std::optional<PropertyTree> readNode(std::istream &input) {
  uint32_t end_offset = readValue<uint32_t>(input);
  uint32_t num_properties = readValue<uint32_t>(input);
  /* property_list_len = */ readValue<uint32_t>(input);
  uint8_t name_len = readValue<uint8_t>(input);

  if (end_offset == 0) return std::nullopt;

  std::string name(name_len, '\0');
  input.read(name.data(), name_len);

  PropertyTree node;
  node.name = std::move(name);

  for (uint32_t i = 0; i < num_properties; i++) {
    absl::StatusOr<Property> property = readProperty(input);
    if (property.ok()) {
      node.properties.push_back(*property);
    } else {
      return std::nullopt;
    }
  }

  while (input.tellg() < end_offset && input.good()) {
    auto child = readNode(input);
    if (child) {
      node.children.push_back(std::move(child.value()));
    } else {
      break;
    }
  }

  input.seekg(end_offset, std::ios::beg);
  return node;
}
