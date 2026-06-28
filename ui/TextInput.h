#pragma once
#include <Arduino.h>
#include "core/UIElement.h"
#include "core/WindowManager.h"   // needs wm.keyboard

class TextInput : public UIElement {
public:
  String text;
  bool isHovered = false;
  bool isFocused = false;
  int  cursorPos = 0;

  TextInput(int _x, int _y, int _w, int _h, bool _rel = false)
    : UIElement(_x, _y, _w, _h, _rel) {}

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX+elW && my >= absY && my <= absY+elH);
    if (clicked) {
      isFocused = isHovered;
      if (isFocused) cursorPos = text.length();
    }
    if (cursorPos > (int)text.length()) cursorPos = text.length();
    if (cursorPos < 0) cursorPos = 0;

    if (isFocused) {
      if (wm.keyboard.backspace && text.length() > 0) {
        text.remove(text.length() - 1);
        cursorPos--;
      }
      for (char c : wm.keyboard.typedChars) {
        if (c == KEY_LEFT)  { if (cursorPos > 0) cursorPos--;                       continue; }
        if (c == KEY_RIGHT) { if (cursorPos < (int)text.length()) cursorPos++;       continue; }
        if (c == KEY_UP || c == KEY_DOWN || c == KEY_ESC) continue;
        String l = text.substring(0, cursorPos);
        String r = text.substring(cursorPos);
        text = l + c + r;
        cursorPos++;
      }
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    uint16_t bg     = canvas->color565(20, 22, 28);
    uint16_t border = isFocused ? TFT_CYAN
                    : isHovered ? TFT_LIGHTGREY
                    :             canvas->color565(80, 80, 80);

    canvas->fillRoundRect(absX, absY, elW, elH, 3, bg);
    canvas->drawRoundRect(absX, absY, elW, elH, 3, border);

    if (!isFocused)
      canvas->drawFastHLine(absX+2, absY+1, elW-4, canvas->color565(10,10,10));
    else
      canvas->drawRoundRect(absX+1, absY+1, elW-2, elH-2, 2, canvas->color565(0,100,150));

    canvas->setTextColor(TFT_WHITE);
    canvas->setTextDatum(middle_left);
    canvas->drawString(text, absX+4, absY+elH/2);
    canvas->setTextDatum(top_left);

    if (isFocused && (millis() % 1000 < 500)) {
      String upTo = text.substring(0, cursorPos);
      int cx = absX + 4 + canvas->textWidth(upTo);
      int ch = 10;
      int cy = absY + elH/2 - ch/2;
      canvas->drawFastVLine(cx, cy, ch, TFT_CYAN);
    }
  }
};
