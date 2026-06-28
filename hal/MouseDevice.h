#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <class/hid/hid.h>
#include "EspUsbHost.h"

// Non-printable ASCII codes used as virtual key symbols
const char KEY_UP    = 0x11;
const char KEY_DOWN  = 0x12;
const char KEY_LEFT  = 0x13;
const char KEY_RIGHT = 0x14;
const char KEY_ESC   = 0x1B;
const char KEY_TAB   = 0x09;

enum SystemEvent {
  SYS_NONE,
  SYS_ALT_TAB,
  SYS_CTRL_ALT_DEL
};

enum PopupType {
  POPUP_INFO,
  POPUP_WARNING,
  POPUP_ERROR
};

// USB HID mouse driver — inherits EspUsbHost so USB interrupts are self-contained
class MouseDevice : public EspUsbHost {
public:
  int x = 120;
  int y = 67;
  int wheel = 0;
  bool leftDown = false;

  void begin() {
    EspUsbHost::begin();
    EspUsbHost::setHIDLocal(HID_LOCAL_US);
  }

  void onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons) override {
    leftDown = (report.buttons & MOUSE_BUTTON_LEFT);
  }

  void onMouseMove(hid_mouse_report_t report) override {
    x += (int8_t)report.x;
    y += (int8_t)report.y;
    wheel = (int8_t)report.wheel;
    if (x < 0)   x = 0;
    if (x > 239) x = 239;
    if (y < 0)   y = 0;
    if (y > 134) y = 134;
  }
};
