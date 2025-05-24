#pragma once

#include <vector>
#include <cstdint>
#include <span>

template <typename T>
std::vector<uint8_t> to_bytes(const std::vector<T> &data) {
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());
  return std::vector<uint8_t>(ptr, ptr + data.size() * sizeof(T));
}

template <typename T, size_t N>
std::vector<uint8_t> to_bytes(const std::array<T, N> &data) {
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data.data());
  return std::vector<uint8_t>(ptr, ptr + data.size() * sizeof(T));
}

template <typename T>
std::span<const uint8_t> to_bytes_view(const T &value) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(&value),
                                  sizeof(T));
}

template <typename T>
std::span<const uint8_t> to_bytes_view(std::span<const T> span) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(
      reinterpret_cast<const uint8_t *>(span.data()), span.size_bytes());
}

template <typename T, size_t N>
std::span<const uint8_t> to_bytes_view(const T (&arr)[N]) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(arr),
                                  sizeof(T) * N);
}

template <typename T>
std::span<const uint8_t> to_bytes_view(const std::vector<T> &vec) {
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");
  return std::span<const uint8_t>(
      reinterpret_cast<const uint8_t *>(vec.data()),
      vec.size() * sizeof(T));
}

#define TRY(...)                                   \
  ({                                               \
    auto res = (__VA_ARGS__);                      \
    if (!res.ok()) return std::move(res).status(); \
    std::move(*res);                               \
  })

#define unused(x) (void)(x)
