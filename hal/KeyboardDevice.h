#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include "hal/MouseDevice.h"   // needs KEY_* and SystemEvent

class KeyboardDevice {
public:
  std::vector<char> typedChars;
  bool backspace   = false;
  bool enter       = false;
  bool fnPressed   = false;
  bool ctrlPressed = false;
  bool altPressed  = false;
  bool tabPressed  = false;
  SystemEvent currentEvent = SYS_NONE;

  char applyFnMap(char c) {
    switch (c) {
      case ';': return KEY_UP;
      case '.': return KEY_DOWN;
      case ',': return KEY_LEFT;
      case '/': return KEY_RIGHT;
      case '`': return KEY_ESC;
      default:  return c;
    }
  }

  void update() {
    typedChars.clear();
    backspace = false;
    enter     = false;

    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();
      fnPressed   = status.fn;
      ctrlPressed = status.ctrl;
      altPressed  = status.alt;
      tabPressed  = status.tab;

      if (status.del)   backspace = true;
      if (status.enter) enter     = true;

      // System hotkeys take priority over normal typing
      if (ctrlPressed && altPressed && status.del) {
        currentEvent = SYS_CTRL_ALT_DEL;
        return;
      }
      if (altPressed && tabPressed) {
        currentEvent = SYS_ALT_TAB;
        return;
      }

      for (auto c : status.word) {
        if (fnPressed) c = applyFnMap(c);
        typedChars.push_back(c);
      }
      if (status.del)   backspace = true;
      if (status.enter) enter     = true;
    }
  }
};
