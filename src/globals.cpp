#include <glm/vec2.hpp>
#include <optional>

static std::optional<glm::ivec2> kScreenWidth = std::nullopt;

void setScreenSize(glm::ivec2 value) {
  kScreenWidth = value;
}

glm::ivec2 getScreenSize() {
  return kScreenWidth.value();
}
