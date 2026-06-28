#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>

class UIElement {
public:
  int  x, y, w, h;
  bool isRelative;

  UIElement(int _x, int _y, int _w, int _h, bool _rel = false)
    : x(_x), y(_y), w(_w), h(_h), isRelative(_rel) {}
  virtual ~UIElement() {}

  virtual void getScreenBounds(int winX, int winY, int winW, int winH,
                               int& outX, int& outY, int& outW, int& outH) {
    outX = winX + (isRelative ? (winW * x / 100) : x);
    outY = winY + y;
    outW = isRelative ? (winW * w / 100) : w;
    outH = h;
  }

  virtual void update(int absX, int absY, int elW, int elH,
                      int mx, int my, bool leftDown,
                      bool clicked, bool released) {}

  virtual void draw(int absX, int absY, int elW, int elH,
                    LGFX_Sprite* canvas) = 0;
};
