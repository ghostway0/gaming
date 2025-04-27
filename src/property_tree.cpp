#include <bit>
#include <istream>
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

std::ostream &operator<<(std::ostream &os, const Property &prop) {
  std::visit(
      [&os](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, int64_t>) {
          os << value;
        } else if constexpr (std::is_same_v<T, float>) {
          os << std::fixed << std::setprecision(2) << value << "f";
        } else if constexpr (std::is_same_v<T, double>) {
          os << std::fixed << std::setprecision(2) << value;
        } else if constexpr (std::is_same_v<T, std::string>) {
          os << "\"" << value << "\"";
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
          os << "[";
          for (size_t i = 0; i < value.size(); ++i) {
            os << static_cast<unsigned int>(value[i]);
            if (i < value.size() - 1) os << ", ";
          }
          os << "]";
        }
      },
      prop);
  return os;
}

} // namespace

std::ostream &operator<<(std::ostream &os, const PropertyTree &tree) {
  struct Printer {
    std::ostream &os;
    int indent_level = 0;
    const int indent_size = 2;

    void print(const PropertyTree &node) {
      os << std::string(indent_level * indent_size, ' ') << node.name
         << ": ";

      for (const auto &prop : node.properties) {
        os << prop << " ";
      }

      os << "{\n";

      if (!node.children.empty()) {
        for (const auto &child : node.children) {
          indent_level++;
          print(child);
          indent_level--;
        }
      }

      os << std::string(indent_level * indent_size, ' ') << "}\n";
    }
  };

  Printer{os}.print(tree);
  return os;
}

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
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid type info 0x%x (offset %zu)", type, input.tellg()));
  }
}

std::ostream &operator<<(std::ostream &os, const PropertyTree &tree);

absl::StatusOr<PropertyTree> readPropertyTree(std::istream &input) {
  uint32_t end_offset = readValue<uint32_t>(input);
  uint32_t num_properties = readValue<uint32_t>(input);
  /* property_list_len = */ readValue<uint32_t>(input);
  uint8_t name_len = readValue<uint8_t>(input);

  if (end_offset == 0) {
    return absl::InvalidArgumentError("Empty property tree");
  }

  size_t start = input.tellg();

  std::string name(name_len, '\0');
  input.read(name.data(), name_len);

  PropertyTree node;
  node.name = std::move(name);

  for (uint32_t i = 0; i < num_properties; i++) {
    absl::StatusOr<Property> property = readProperty(input);
    if (property.ok()) {
      node.properties.push_back(*property);
    } else {
      return property.status();
    }
  }

  while (input.tellg() < end_offset + start && input.good()) {
    auto child = readPropertyTree(input);
    if (child.ok()) {
      node.children.push_back(std::move(child.value()));
    } else {
      return child.status();
    }
  }

  input.seekg(start + end_offset, std::ios::beg);
  return node;
}
