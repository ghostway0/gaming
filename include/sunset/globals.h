#pragma once

#include <optional>
#include <string>

#include <glm/fwd.hpp>

struct ScreenSizeTag {};
struct CurrentExecutableTag {};

template <typename Tag, typename T>
class GlobalValue {
 private:
  static std::optional<T> value_;

 public:
  static void set(T v) { value_ = std::move(v); }

  static T get() { return value_.value(); }

  static bool has_value() { return value_.has_value(); }

  static void reset() { value_.reset(); }
};

template <typename Tag, typename T>
inline std::optional<T> GlobalValue<Tag, T>::value_ = std::nullopt;

using kScreenSize = GlobalValue<ScreenSizeTag, glm::ivec2>;
using kCurrentExec = GlobalValue<CurrentExecutableTag, std::string>;
