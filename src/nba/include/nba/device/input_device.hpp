/*
 * Copyright (C) 2023 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <functional>

namespace nba {

struct InputDevice {
  virtual ~InputDevice() = default;

  enum class Key {
    Up,
    Down,
    Left,
    Right,
    Start,
    Select,
    A,
    B,
    L,
    R
  };

  static constexpr int kKeyCount = 10;
  
  virtual auto Poll(Key key) -> bool = 0;
  virtual void SetOnChangeCallback(std::function<void(void)> callback) = 0;
};

struct NullInputDevice : InputDevice {
  auto Poll(Key key) -> bool final {
    return false;
  }

  void SetOnChangeCallback(std::function<void(void)> callback) final {}
};

struct BasicInputDevice : InputDevice {
  void SetKeyStatus(Key key, bool pressed) {
    key_status[static_cast<int>(key)] = pressed;
    keypress_callback();
  }

  auto Poll(Key key) -> bool final {
    return key_status[static_cast<int>(key)];
  }

  void SetOnChangeCallback(std::function<void(void)> callback) final {
    keypress_callback = callback;
  }
private:
  std::function<void(void)> keypress_callback;
  bool key_status[kKeyCount];
};

} // namespace nba
