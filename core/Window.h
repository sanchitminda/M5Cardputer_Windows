#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include <functional>
#include "core/UIElement.h"

// Forward declaration — WindowManager is not needed for Window's
// data members or simple methods; update/draw bodies are in WindowImpl.h
class WindowManager;

// Single authoritative declaration of the global kernel instance.
// Every file that includes this header (directly or via MicroWin.h) sees it.
extern WindowManager wm;

class Window {
public:
  int x, y, w, h;
  String   title;
  uint16_t bgColor;
  std::vector<UIElement*> elements;
  QueueHandle_t ipcQueue;

  int scrollY       = 0;
  int contentHeight = 0;

  bool isDragging  = false;
  bool isResizing  = false;
  int  dragOffsetX = 0;
  int  dragOffsetY = 0;

  bool isMinimized = false;
  int  normalH     = 0;

  bool isMaximized = false;
  int  normX, normY, normW, normH;

  std::function<void()>        deferredTask = nullptr;
  std::function<void(Window*)> onUpdate     = nullptr;
  std::function<void()>        onClose      = nullptr;

  Window(int _x, int _y, int _w, int _h, String _title, uint16_t _bg)
    : x(_x), y(_y), w(_w), h(_h), title(_title), bgColor(_bg)
  {
    ipcQueue = xQueueCreate(10, sizeof(char[64]));
  }

  ~Window() {
    if (onClose) onClose();
    for (auto el : elements) delete el;
    elements.clear();
    if (ipcQueue) vQueueDelete(ipcQueue);
  }

  void clearElements() {
    for (auto el : elements) delete el;
    elements.clear();
    scrollY       = 0;
    contentHeight = 0;
  }

  void addElement(UIElement* el) { elements.push_back(el); }

  void computeContentHeight() {
    int maxH = h - 15;
    for (auto el : elements) {
      int eX, eY, eW, eH;
      el->getScreenBounds(0, 0, w, h - 15, eX, eY, eW, eH);
      if (eY + eH + 15 > maxH) maxH = eY + eH + 15;
    }
    contentHeight = maxH;
  }

  // Bodies live in core/WindowImpl.h (needs full WindowManager definition)
  void update(int mx, int my, int wheel, bool leftDown,
              bool clicked, bool released, LGFX_Sprite* canvas);
  void draw(LGFX_Sprite* canvas);
};
