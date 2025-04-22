#pragma once

#include <bitset>

#include "sunset/event_queue.h"

// clang-format off
enum class Key {
  Unknown = 0,
  A, B, C, D, E, F, G,
  H, I, J, K, L, M, N,
  O, P, Q, R, S, T, U,
  V, W, X, Y, Z,

  Num0, Num1, Num2, Num3, Num4,
  Num5, Num6, Num7, Num8, Num9,

  Escape, Enter, Tab, Backspace, Space,
  Left, Right, Up, Down,

  LShift, RShift, LCtrl, RCtrl, LAlt, RAlt,
  LMeta, RMeta,

  Insert, Delete, Home, End, PageUp, PageDown,

  F1, F2, F3, F4, F5, F6, F7, F8,
  F9, F10, F11, F12,

  COUNT
};
// clang-format on

enum Modifier : uint8_t {
  ModNone = 0,
  ModShift = 1 << 0,
  ModCtrl = 1 << 1,
  ModAlt = 1 << 2,
  ModMeta = 1 << 3
};

inline Modifier operator|(Modifier a, Modifier b) {
  return static_cast<Modifier>(static_cast<uint8_t>(a) |
                               static_cast<uint8_t>(b));
}

struct KeyDown {
  Key key;
  Modifier mods;
};

struct KeyUp {
  Key key;
  Modifier mods;
};

struct KeyHeld {
  Key key;
  Modifier mods;
};

struct KeyPressed {
  std::bitset<static_cast<size_t>(Key::COUNT)> map;
};
struct MouseDown {
  int button;
  int mods;
};

struct MouseUp {
  int button;
  int mods;
};

struct MouseMoved {
  double x;
  double y;
  double dx;
  double dy;
};

struct MouseScrolled {
  double dx;
  double dy;
};

class IOProvider {
  virtual void poll(EventQueue &event_queue) = 0;
};
