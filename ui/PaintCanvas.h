#pragma once
#include <Arduino.h>
#include "core/UIElement.h"

class PaintCanvas : public UIElement {
public:
  LGFX_Sprite* buffer;
  uint16_t*    currentColor;
  int lastX = -1;
  int lastY = -1;

  PaintCanvas(int _x, int _y, int _w, int _h, uint16_t* _colorRef)
    : UIElement(_x, _y, _w, _h, false), currentColor(_colorRef)
  {
    buffer = new LGFX_Sprite();
    buffer->setColorDepth(16);
    buffer->createSprite(_w, _h);
    buffer->fillSprite(TFT_WHITE);
  }

  ~PaintCanvas() { buffer->deleteSprite(); delete buffer; }

  void clear() { buffer->fillSprite(TFT_WHITE); }

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool /*clicked*/, bool /*released*/) override {
    bool hover = (mx >= absX && mx <= absX+elW && my >= absY && my <= absY+elH);
    if (hover && leftDown) {
      int lx = mx-absX, ly = my-absY;
      if (lastX != -1) {
        buffer->drawLine(lastX,   lastY,   lx,   ly,   *currentColor);
        buffer->drawLine(lastX+1, lastY,   lx+1, ly,   *currentColor);
        buffer->drawLine(lastX,   lastY+1, lx,   ly+1, *currentColor);
      } else {
        buffer->fillCircle(lx, ly, 1, *currentColor);
      }
      lastX = lx; lastY = ly;
    } else {
      lastX = -1; lastY = -1;
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    buffer->pushSprite(canvas, absX, absY);
    canvas->drawFastHLine(absX, absY, elW, canvas->color565(100,100,100));
    canvas->drawFastVLine(absX, absY, elH, canvas->color565(100,100,100));
    canvas->drawRect(absX, absY, elW, elH, TFT_DARKGREY);
  }
};
