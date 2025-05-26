#include <bit>
#include <istream>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>
#include <absl/status/statusor.h>
#include <absl/status/status_matchers.h>
#include <zlib.h>

#include "sunset/utils.h"

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
void printValue(std::ostream &os, const T &value) {
  if constexpr (std::is_integral_v<T>) {
    os << value;
  } else if constexpr (std::is_same_v<T, float>) {
    os << std::fixed << std::setprecision(2) << value << "f";
  } else if constexpr (std::is_same_v<T, double>) {
    os << std::fixed << std::setprecision(2) << value;
  } else if constexpr (std::is_same_v<T, std::string>) {
    os << "\"" << value << "\"";
  } else {
    os << value;
  }
}

std::ostream &operator<<(std::ostream &os, const Property &prop) {
  std::visit(
      [&os](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (is_vector_v<T>) {
          os << "[";
          for (size_t i = 0; i < value.size(); ++i) {
            printValue(os, value[i]);
            if (i < value.size() - 1) os << ", ";
          }
          os << "]";
        } else {
          printValue(os, value);
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

PropertyTree *PropertyTree::getNodeByName(std::string_view name, size_t i) {
  PropertyTree *current = this;

  for (const std::string_view &sub : absl::StrSplit(name, ".")) {
    bool found = false;
    for (PropertyTree &child : current->children) {
      if (child.name == sub) {
        current = &child;
        if (i-- == 0) {
          found = true;
          break;
        }
      }
    }

    if (!found) {
      return nullptr;
    }
  }

  return current;
}

PropertyTree const *PropertyTree::getNodeByName(std::string_view name,
                                                size_t i) const {
  return const_cast<PropertyTree *>(this)->getNodeByName(name);
}

template <typename T>
absl::StatusOr<std::vector<T>> readArrayProperty(std::istream &input) {
  uint32_t array_len = readValue<uint32_t>(input);
  uint32_t encoding = readValue<uint32_t>(input);
  uint32_t compressed_len = readValue<uint32_t>(input);
  std::vector<uint8_t> data(compressed_len);
  input.read(reinterpret_cast<char *>(data.data()), compressed_len);

  std::vector<uint8_t> bytes;
  if (encoding == 1) {
    bytes = TRY(decompressData(data, array_len * sizeof(T)));
  } else if (encoding == 0) {
    bytes = std::move(data);
  } else {
    return absl::UnimplementedError(
        absl::StrFormat("Encoding %d not yet implemented", encoding));
  }

  if (bytes.size() != array_len * sizeof(T)) {
    return absl::InternalError("Unexpected decompressed size");
  }

  static_assert(alignof(T) <= alignof(std::max_align_t),
                "T alignment might be violated");

  std::vector<T> result;
  result.resize(array_len);
  std::memcpy(result.data(), bytes.data(), bytes.size());

  return result;
}

absl::StatusOr<Property> readProperty(std::istream &input) {
  char type = readValue<char>(input);
  switch (type) {
    case 'C':
      return readValue<uint8_t>(input);
    case 'Y':
      return readValue<int16_t>(input);
    case 'I':
      return readValue<int32_t>(input);
    case 'L':
      return readValue<int64_t>(input);
    case 'F':
      return readValue<float>(input);
    case 'D':
      return readValue<double>(input);
    case 'S': {
      uint32_t len = readValue<uint32_t>(input);
      std::string data(len, '\0');
      input.read(data.data(), len);
      return data;
    }
    case 'c':
      return readArrayProperty<uint8_t>(input);
    case 'i':
      return readArrayProperty<int32_t>(input);
    case 'l':
      return readArrayProperty<int64_t>(input);
    case 'f':
      return readArrayProperty<float>(input);
    case 'd':
      return readArrayProperty<double>(input);
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
    node.properties.push_back(TRY(readProperty(input)));
  }

  while (input.tellg() < end_offset + start && input.good()) {
    node.children.push_back(TRY(readPropertyTree(input)));
  }

  input.seekg(start + end_offset, std::ios::beg);
  return node;
}
