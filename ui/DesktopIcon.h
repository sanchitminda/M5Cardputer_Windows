#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <functional>
#include "core/Window.h"

class DesktopIcon {
public:
  int x, y;
  String name;
  void (*iconFunc)(LGFX_Sprite*, int, int) = nullptr;
  std::function<Window*()> onClick;

  DesktopIcon(int _x, int _y, String _name,
              void (*_iconFunc)(LGFX_Sprite*, int, int),
              std::function<Window*()> _onClick)
    : x(_x), y(_y), name(_name), iconFunc(_iconFunc), onClick(_onClick) {}

  bool checkClick(int mx, int my) {
    return (mx >= x && mx <= x + 32 && my >= y && my <= y + 32);
  }

  void draw(LGFX_Sprite* canvas) {
    if (iconFunc) iconFunc(canvas, x + 4, y + 4);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    canvas->setTextDatum(top_center);
    canvas->drawString(name, x + 16, y + 34);
    canvas->setTextDatum(top_left);
  }
};
