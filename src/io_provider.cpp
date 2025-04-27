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

namespace {

// clang-format off
Key glfwToKey(int glfw_key) {
  switch (glfw_key) {
    case GLFW_KEY_A: return Key::A;
    case GLFW_KEY_B: return Key::B;
    case GLFW_KEY_C: return Key::C;
    case GLFW_KEY_D: return Key::D;
    case GLFW_KEY_E: return Key::E;
    case GLFW_KEY_F: return Key::F;
    case GLFW_KEY_G: return Key::G;
    case GLFW_KEY_H: return Key::H;
    case GLFW_KEY_I: return Key::I;
    case GLFW_KEY_J: return Key::J;
    case GLFW_KEY_K: return Key::K;
    case GLFW_KEY_L: return Key::L;
    case GLFW_KEY_M: return Key::M;
    case GLFW_KEY_N: return Key::N;
    case GLFW_KEY_O: return Key::O;
    case GLFW_KEY_P: return Key::P;
    case GLFW_KEY_Q: return Key::Q;
    case GLFW_KEY_R: return Key::R;
    case GLFW_KEY_S: return Key::S;
    case GLFW_KEY_T: return Key::T;
    case GLFW_KEY_U: return Key::U;
    case GLFW_KEY_V: return Key::V;
    case GLFW_KEY_W: return Key::W;
    case GLFW_KEY_X: return Key::X;
    case GLFW_KEY_Y: return Key::Y;
    case GLFW_KEY_Z: return Key::Z;

    case GLFW_KEY_0: return Key::Num0;
    case GLFW_KEY_1: return Key::Num1;
    case GLFW_KEY_2: return Key::Num2;
    case GLFW_KEY_3: return Key::Num3;
    case GLFW_KEY_4: return Key::Num4;
    case GLFW_KEY_5: return Key::Num5;
    case GLFW_KEY_6: return Key::Num6;
    case GLFW_KEY_7: return Key::Num7;
    case GLFW_KEY_8: return Key::Num8;
    case GLFW_KEY_9: return Key::Num9;

    case GLFW_KEY_ESCAPE: return Key::Escape;
    case GLFW_KEY_ENTER: return Key::Enter;
    case GLFW_KEY_TAB: return Key::Tab;
    case GLFW_KEY_BACKSPACE: return Key::Backspace;
    case GLFW_KEY_SPACE: return Key::Space;

    case GLFW_KEY_LEFT: return Key::Left;
    case GLFW_KEY_RIGHT: return Key::Right;
    case GLFW_KEY_UP: return Key::Up;
    case GLFW_KEY_DOWN: return Key::Down;

    case GLFW_KEY_LEFT_SHIFT: return Key::LShift;
    case GLFW_KEY_RIGHT_SHIFT: return Key::RShift;
    case GLFW_KEY_LEFT_CONTROL: return Key::LCtrl;
    case GLFW_KEY_RIGHT_CONTROL: return Key::RCtrl;
    case GLFW_KEY_LEFT_ALT: return Key::LAlt;
    case GLFW_KEY_RIGHT_ALT: return Key::RAlt;
    case GLFW_KEY_LEFT_SUPER: return Key::LMeta;
    case GLFW_KEY_RIGHT_SUPER: return Key::RMeta;

    case GLFW_KEY_INSERT: return Key::Insert;
    case GLFW_KEY_DELETE: return Key::Delete;
    case GLFW_KEY_HOME: return Key::Home;
    case GLFW_KEY_END: return Key::End;
    case GLFW_KEY_PAGE_UP: return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key::PageDown;

    case GLFW_KEY_F1: return Key::F1;
    case GLFW_KEY_F2: return Key::F2;
    case GLFW_KEY_F3: return Key::F3;
    case GLFW_KEY_F4: return Key::F4;
    case GLFW_KEY_F5: return Key::F5;
    case GLFW_KEY_F6: return Key::F6;
    case GLFW_KEY_F7: return Key::F7;
    case GLFW_KEY_F8: return Key::F8;
    case GLFW_KEY_F9: return Key::F9;
    case GLFW_KEY_F10: return Key::F10;
    case GLFW_KEY_F11: return Key::F11;
    case GLFW_KEY_F12: return Key::F12;

    default: return Key::Unknown;
  }
}
// clang-format on

} // namespace

GLFWIO::GLFWIO(EventQueue &q) : queue_(q) {
  if (!glfwInit()) {
    return;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window = glfwCreateWindow(800, 600, "", nullptr, nullptr);
  if (!window) {
    return;
  }

  glfwMakeContextCurrent(window);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  // glEnable(GL_DEBUG_OUTPUT);

  glewInit();

  glfwSetWindowUserPointer(window, this);

  glfwSetKeyCallback(window, [](GLFWwindow *win, int glfw_key, int scancode,
                                int action, int mods) {
    auto *self = static_cast<GLFWIO *>(glfwGetWindowUserPointer(win));
    Key key = glfwToKey(glfw_key);
    Modifier m = Modifier::ModNone;
    if (mods & GLFW_MOD_SHIFT) m = m | Modifier::ModShift;
    if (mods & GLFW_MOD_CONTROL) m = m | Modifier::ModCtrl;
    if (mods & GLFW_MOD_ALT) m = m | Modifier::ModAlt;
    if (mods & GLFW_MOD_SUPER) m = m | Modifier::ModMeta;

    if (action == GLFW_PRESS) {
      self->queue_.send(KeyDown{key, m});
      self->key_state_.set(static_cast<size_t>(key));
    } else if (action == GLFW_RELEASE) {
      self->queue_.send(KeyUp{key, m});
      self->key_state_.reset(static_cast<size_t>(key));
    }
  });

  glfwSetMouseButtonCallback(
      window, [](GLFWwindow *win, int button, int action, int mods) {
        auto *self = static_cast<GLFWIO *>(glfwGetWindowUserPointer(win));
        if (action == GLFW_PRESS) {
          self->queue_.send(MouseDown{button});
          self->mouse_state_.set(button);
        } else if (action == GLFW_RELEASE) {
          self->queue_.send(MouseUp{button});
          self->mouse_state_.reset(button);
        }
      });

  glfwSetScrollCallback(
      window, [](GLFWwindow *win, double xoffset, double yoffset) {
        auto *self = static_cast<GLFWIO *>(glfwGetWindowUserPointer(win));
        self->queue_.send(MouseScrolled{xoffset, yoffset});
      });

  window_ = window;
  valid_ = true;
}

GLFWIO::~GLFWIO() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

bool GLFWIO::poll(EventQueue &event_queue) {
  glfwPollEvents();

  double x, y;
  glfwGetCursorPos(window_, &x, &y);
  if (first_mouse_) {
    last_x_ = x;
    last_y_ = y;
    first_mouse_ = false;
  } else if (x != last_x_ || y != last_y_) {
    event_queue.send(MouseMoved{x, y, x - last_x_, y - last_y_});
    last_x_ = x;
    last_y_ = y;
  }
  event_queue.send(KeyPressed{key_state_});

  glfwSwapBuffers(window_);

  return !glfwWindowShouldClose(window_);
}
