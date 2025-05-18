#pragma once

#include <filesystem>

class Config {
 public:
  static Config &instance() {
    static Config config;
    return config;
  }

  void setInternalPath(std::filesystem::path p) {
    internal_path_ = std::move(p);
  }

  const std::filesystem::path &getInternalPath() const {
    return internal_path_;
  }

 private:
  std::filesystem::path internal_path_;
  Config() = default;
};
