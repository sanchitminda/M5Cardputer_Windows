#pragma once
#include <Arduino.h>
#include <functional>
#include "core/UIElement.h"

class Button : public UIElement {
public:
  String text;
  std::function<void()> onClick;
  bool isHovered = false;
  bool isPressed = false;

  Button(int _x, int _y, int _w, int _h, String _text, bool _rel = false)
    : UIElement(_x, _y, _w, _h, _rel), text(_text) {}

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    isPressed = (isHovered && leftDown);
    if (isHovered && released && onClick) onClick();
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    if (isPressed) {
      int px = absX + 2, py = absY + 2;
      canvas->fillRoundRect(px, py, elW, elH, 3, canvas->color565(100, 100, 100));
      canvas->drawRoundRect(px, py, elW, elH, 3, TFT_DARKGREY);
      canvas->setTextColor(TFT_WHITE);
      canvas->setTextDatum(middle_center);
      canvas->drawString(text, px + elW / 2, py + elH / 2);
      canvas->setTextDatum(top_left);
    } else {
      canvas->fillRoundRect(absX + 2, absY + 2, elW, elH, 3, canvas->color565(30, 30, 30));
      uint16_t bg = isHovered ? TFT_LIGHTGREY : TFT_DARKGREY;
      canvas->fillRoundRect(absX, absY, elW, elH, 3, bg);
      canvas->drawRoundRect(absX, absY, elW, elH, 3, TFT_WHITE);
      canvas->setTextColor(TFT_WHITE);
      canvas->setTextDatum(middle_center);
      canvas->drawString(text, absX + elW / 2, absY + elH / 2);
      canvas->setTextDatum(top_left);
    }
  }
};
