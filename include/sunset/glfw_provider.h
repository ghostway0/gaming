#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstddef>

#include <absl/status/statusor.h>

#include "sunset/io_provider.h"

class GLFWIO : public IOProvider {
 public:
  GLFWIO(EventQueue &q);

  ~GLFWIO() override;

  bool poll(EventQueue &event_queue) override;

  bool valid() override { return valid_; };

 private:
  GLFWwindow *window_;
  EventQueue &queue_;

  std::bitset<static_cast<size_t>(Key::COUNT)> key_state_;
  std::bitset<GLFW_MOUSE_BUTTON_LAST + 1> mouse_state_;
  double last_x_ = 0, last_y_ = 0;
  bool first_mouse_{true};

  bool valid_{false};
};
