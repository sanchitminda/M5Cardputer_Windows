#pragma once
#include <Arduino.h>
#include "core/UIElement.h"

class Label : public UIElement {
public:
  String   text;
  uint16_t color;

  Label(int _x, int _y, String _text, uint16_t _color, bool _rel = false)
    : UIElement(_x, _y, 0, 10, _rel), text(_text), color(_color) {}

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    canvas->setTextColor(color);
    canvas->setTextSize(1);
    canvas->setCursor(absX, absY);
    canvas->print(text);
  }
};
