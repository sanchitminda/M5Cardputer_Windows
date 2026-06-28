#pragma once
// Window::update() and Window::draw() out-of-line definitions.
// Must be included AFTER core/WindowManager.h so wm.closeWindow() resolves.
#include "core/Window.h"
#include "core/WindowManager.h"

inline void Window::update(int mx, int my, int wheel, bool leftDown,
                            bool clicked, bool released, LGFX_Sprite* canvas) {
  if (onUpdate) onUpdate(this);

  int titleH = 18;

  if (released) { isDragging = false; isResizing = false; }

  if (clicked) {
    // Close button [X]
    if (mx >= x+w-20 && mx <= x+w-4 && my >= y && my <= y+titleH) {
      wm.closeWindow(this); return;
    }
    // Maximize button [O]
    if (mx >= x+w-36 && mx < x+w-20 && my >= y && my <= y+titleH) {
      if (isMaximized) {
        x=normX; y=normY; w=normW; h=normH; isMaximized=false;
      } else {
        normX=x; normY=y; normW=w; normH=h;
        x=0; y=0; w=240; h=135; isMaximized=true; isMinimized=false;
      }
      return;
    }
    // Minimize button [-]
    if (mx >= x+w-52 && mx < x+w-36 && my >= y && my <= y+titleH) {
      if (isMinimized) { h=normalH; isMinimized=false; }
      else             { normalH=h; h=titleH; isMinimized=true; }
      return;
    }
  }

  // Drag (title bar, excluding buttons)
  if (clicked && !isMaximized && my >= y && my < y+titleH && mx >= x && mx < x+w-52) {
    isDragging = true; dragOffsetX = mx-x; dragOffsetY = my-y;
  }
  if (isDragging && leftDown) {
    x = mx-dragOffsetX; y = my-dragOffsetY;
    if (y < 0) y = 0;
  }

  // Resize (bottom-right corner)
  if (clicked && !isMaximized && !isMinimized &&
      mx >= x+w-15 && mx <= x+w && my >= y+h-15 && my <= y+h) {
    isResizing = true;
  }
  if (isResizing && leftDown) {
    w = mx-x; h = my-y;
    if (w < 80)  w = 80;
    if (h < 40)  h = 40;
  }

  if (isMinimized) return;

  // Mouse-wheel scrolling
  if (wheel != 0) {
    computeContentHeight();
    int maxScroll = contentHeight - (h - titleH);
    if (maxScroll < 0) maxScroll = 0;
    scrollY -= wheel * 15;
    if (scrollY < 0) scrollY = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;
  }

  for (auto el : elements) {
    int eX, eY, eW, eH;
    el->getScreenBounds(x, y+titleH-scrollY, w, h-titleH, eX, eY, eW, eH);
    el->update(eX, eY, eW, eH, mx, my, leftDown, clicked, released);
  }
}

inline void Window::draw(LGFX_Sprite* canvas) {
  int titleH = 18;

  // Drop shadow
  if (!isMaximized)
    canvas->fillRoundRect(x+4, y+4, w, h, 6, canvas->color565(15, 15, 15));

  // Background
  canvas->fillRoundRect(x, y, w, h, 6, bgColor);

  // Title bar
  uint16_t titleColor = canvas->color565(40, 45, 55);
  canvas->fillRoundRect(x, y, w, titleH, 6, titleColor);
  if (!isMinimized)
    canvas->fillRect(x, y+10, w, titleH-10, titleColor);

  // Border
  canvas->drawRoundRect(x, y, w, h, 6, canvas->color565(90, 90, 90));
  canvas->drawFastHLine(x, y+titleH, w, canvas->color565(60, 60, 60));

  // Title text
  canvas->setTextColor(TFT_WHITE);
  canvas->setTextSize(1);
  canvas->setTextDatum(middle_left);
  canvas->drawString(title, x+8, y+titleH/2);
  canvas->setTextDatum(top_left);

  // Traffic-light buttons
  canvas->fillCircle(x+w-12, y+titleH/2, 5, canvas->color565(255, 96,  92));
  canvas->fillCircle(x+w-28, y+titleH/2, 5, canvas->color565( 38,201,  64));
  canvas->fillCircle(x+w-44, y+titleH/2, 5, canvas->color565(255,189,  46));

  if (isMinimized) return;

  // Clip + elements
  canvas->setClipRect(x+1, y+titleH+1, w-2, h-titleH-2);
  for (auto el : elements) {
    int eX, eY, eW, eH;
    el->getScreenBounds(x, y+titleH-scrollY, w, h-titleH, eX, eY, eW, eH);
    if (eY+eH > y+titleH && eY < y+h)
      el->draw(eX, eY, eW, eH, canvas);
  }
  canvas->clearClipRect();

  // Scrollbar
  computeContentHeight();
  int maxScroll = contentHeight - (h-titleH);
  if (maxScroll > 0) {
    int thumbH = max((h-titleH)*(h-titleH)/contentHeight, 15);
    int thumbY = y+titleH+2 + (scrollY*(h-titleH-thumbH-4)/maxScroll);
    canvas->fillRoundRect(x+w-6, y+titleH+2, 4, h-titleH-4, 2, canvas->color565(30,30,30));
    canvas->fillRoundRect(x+w-6, thumbY, 4, thumbH, 2, canvas->color565(150,150,150));
  }

  // Resize grabber
  if (!isMaximized) {
    uint16_t g = canvas->color565(120, 120, 120);
    canvas->drawLine(x+w-4, y+h-10, x+w-10, y+h-4, g);
    canvas->drawLine(x+w-4, y+h-6,  x+w-6,  y+h-4, g);
  }

  if (deferredTask) { deferredTask(); deferredTask = nullptr; }
}
