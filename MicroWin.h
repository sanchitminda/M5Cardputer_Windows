#ifndef MICROWIN_H
#define MICROWIN_H

#include <Arduino.h>
#include "MicroMouse.h"
#include <M5Cardputer.h>
#include <vector>
#include <functional>

// --- Added for ESP32-S3 ROM Bootloader Restart ---
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"

// ==========================================
// 1. GLOBAL STATE & USB HOST
// ==========================================
LGFX_Sprite canvas(&M5Cardputer.Display);


MyEspUsbHost usbHost;

// ==========================================
// 2. UI ELEMENT BASE & COMPONENTS
// ==========================================
class UIElement {
public:
  int x, y, w, h;
  bool isRelative;
  bool isFocused = false;
  bool isFocusable = false;

  UIElement(int _x, int _y, int _w, int _h, bool _isRel = false) 
    : x(_x), y(_y), w(_w), h(_h), isRelative(_isRel) {}

  // VIRTUAL DESTRUCTOR: Crucial for freeing RAM of derived classes (like PaintCanvas vectors)
  virtual ~UIElement() {}

  void getScreenBounds(int winX, int winY, int winW, int winH, int &outX, int &outY, int &outW, int &outH) {
    if (isRelative) {
      outX = winX + (winW * x / 100); outY = winY + (winH * y / 100);
      outW = (winW * w / 100); outH = (winH * h / 100);
    } else {
      outX = winX + x; outY = winY + y; outW = w; outH = h;
    }
  }

  virtual void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) {}
  virtual void draw(int absX, int absY, int elW, int elH) {}
  virtual void handleKeyboard(char key) {}
};

class Button : public UIElement {
public:
  String text;
  std::function<void()> onClick;
  bool isHovered = false;
  bool isPressed = false;

  Button(int _x, int _y, int _w, int _h, String _text, bool _rel = false) 
    : UIElement(_x, _y, _w, _h, _rel), text(_text), onClick(nullptr) {}

  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    if (isHovered && clicked) isPressed = true;
    if (!leftDown) isPressed = false;
    if (isHovered && released && onClick) onClick();
  }

  void draw(int absX, int absY, int elW, int elH) override {
    uint16_t color = isPressed ? TFT_DARKGREY : (isHovered ? TFT_LIGHTGREY : TFT_WHITE);
    canvas.fillRect(absX, absY, elW, elH, color);
    canvas.drawRect(absX, absY, elW, elH, TFT_BLACK);
    canvas.setTextColor(TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setCursor(absX + 4, absY + 4);
    canvas.print(text);
  }
};

class Label : public UIElement {
public:
  String text;
  uint16_t color;
  Label(int _x, int _y, String _text, uint16_t _color = TFT_WHITE, bool _rel = false) 
    : UIElement(_x, _y, 0, 0, _rel), text(_text), color(_color) {}
  void draw(int absX, int absY, int elW, int elH) override {
    canvas.setTextColor(color); canvas.setTextSize(1);
    canvas.setCursor(absX, absY); canvas.print(text);
  }
};

class TextInput : public UIElement {
public:
  String text = "";
  TextInput(int _x, int _y, int _w, int _h, bool _rel = false) : UIElement(_x, _y, _w, _h, _rel) { isFocusable = true; }
  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    if (clicked) isFocused = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
  }
  void handleKeyboard(char key) override {
    if (key == '\b' && text.length() > 0) text.remove(text.length() - 1);
    else if (key >= 32 && key <= 126) text += key;
  }
  void draw(int absX, int absY, int elW, int elH) override {
    canvas.fillRect(absX, absY, elW, elH, TFT_WHITE);
    canvas.drawRect(absX, absY, elW, elH, isFocused ? TFT_BLUE : TFT_DARKGREY);
    canvas.setTextColor(TFT_BLACK); canvas.setTextSize(1); canvas.setCursor(absX + 4, absY + 4);
    canvas.print(isFocused && (millis() / 500) % 2 == 0 ? text + "|" : text);
  }
};

// The Paint Application Canvas
class PaintCanvas : public UIElement {
public:
  struct Line { int x0, y0, x1, y1; };
  std::vector<Line> lines; 
  int lastMx = -1, lastMy = -1;

  PaintCanvas(int _x, int _y, int _w, int _h, bool _rel = false) : UIElement(_x, _y, _w, _h, _rel) {}
  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    if (leftDown && (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH)) {
      if (lastMx != -1) lines.push_back({lastMx - absX, lastMy - absY, mx - absX, my - absY});
      lastMx = mx; lastMy = my;
    } else {
      lastMx = -1; lastMy = -1;
    }
  }
  void draw(int absX, int absY, int elW, int elH) override {
    canvas.fillRect(absX, absY, elW, elH, TFT_WHITE);
    canvas.drawRect(absX, absY, elW, elH, TFT_DARKGREY);
    for (auto& l : lines) canvas.drawLine(absX + l.x0, absY + l.y0, absX + l.x1, absY + l.y1, TFT_BLACK);
  }
};

// ==========================================
// 3. WINDOW CLASS
// ==========================================
class Window {
public:
  int x, y, w, h;
  int restoreX, restoreY, restoreW, restoreH;
  String title;
  uint16_t bgColor;
  std::vector<UIElement*> elements;
  std::function<void(Window*)> onUpdate;

  bool isOpen = false; 
  bool isClosed = false;
  bool isMaximized = false;
  bool isDragging = false;
  bool isResizing = false;
  int dragOffX = 0, dragOffY = 0;

  Window(int _x, int _y, int _w, int _h, String _title, uint16_t _bg) 
    : x(_x), y(_y), w(_w), h(_h), title(_title), bgColor(_bg) {}

  ~Window() {
    for (auto el : elements) {
      delete el; 
    }
    elements.clear();
  }
  void addElement(UIElement* el) { elements.push_back(el); }
  void handleKeyboard(char key) { for (auto el : elements) if (el->isFocused) el->handleKeyboard(key); }

  bool update(int mx, int my, bool leftDown, bool clicked, bool released) {
    if (!isOpen || isClosed) return false;
    if (onUpdate) onUpdate(this);

    int titleH = 15;
    int btnW = 15;

    // 1. Title Bar Buttons
    if (clicked && my >= y && my <= y + titleH) {
      if (mx >= x + w - btnW) { 
        isClosed = true; // Mark for RAM cleanup!
        return true; 
      } 
      else if (mx >= x + w - btnW * 2) { 
        if (!isMaximized) {
          restoreX = x; restoreY = y; restoreW = w; restoreH = h;
          x = 0; y = 0; w = 240; h = 135;
        } else {
          x = restoreX; y = restoreY; w = restoreW; h = restoreH;
        }
        isMaximized = !isMaximized; return true;
      } 
      else if (mx >= x + w - btnW * 3) { 
        isOpen = false; // Just hide it, keep in RAM
        return true; 
      }
    }

    // 2. Dragging & Resizing
    if (!isMaximized) {
      if (clicked && mx >= x + w - 12 && mx <= x + w && my >= y + h - 12 && my <= y + h) isResizing = true;
      if (isResizing) {
        w = mx - x; h = my - y;
        if (w < 80) w = 80; if (h < 40) h = 40;
        if (released) isResizing = false;
        return true;
      }
      if (clicked && mx >= x && mx <= x + w - btnW*3 && my >= y && my <= y + titleH) {
        isDragging = true; dragOffX = mx - x; dragOffY = my - y;
      }
      if (isDragging) {
        x = mx - dragOffX; y = my - dragOffY;
        if (released) isDragging = false;
        return true;
      }
    }

    // 3. UI Elements Update
    if (mx >= x && mx <= x + w && my >= y && my <= y + h && !isDragging && !isResizing) {
      for (auto el : elements) {
        int eX, eY, eW, eH;
        el->getScreenBounds(x, y + titleH, w, h - titleH, eX, eY, eW, eH);
        el->update(eX, eY, eW, eH, mx, my, leftDown, clicked, released);
      }
      return true; 
    }
    return false;
  }

  void draw(bool isFocused) {
    if (!isOpen || isClosed) return;
    int titleH = 15, btnW = 15;
    canvas.fillRect(x, y, w, h, bgColor);
    canvas.drawRect(x, y, w, h, isFocused ? TFT_YELLOW : TFT_WHITE); 
    
    // Title Bar
    canvas.fillRect(x, y, w, titleH, isFocused ? TFT_NAVY : TFT_DARKGREY);
    canvas.drawFastHLine(x, y + titleH, w, isFocused ? TFT_YELLOW : TFT_WHITE);
    canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1); canvas.setCursor(x + 4, y + 4); canvas.print(title);

    // Title Bar Buttons (-, [], X)
    canvas.fillRect(x + w - btnW*3, y, btnW*3, titleH, TFT_LIGHTGREY); 
    canvas.drawRect(x + w - btnW*3, y, btnW, titleH, TFT_BLACK); canvas.drawLine(x + w - btnW*3 + 3, y + titleH - 5, x + w - btnW*2 - 4, y + titleH - 5, TFT_BLACK); 
    canvas.drawRect(x + w - btnW*2, y, btnW, titleH, TFT_BLACK); canvas.drawRect(x + w - btnW*2 + 3, y + 3, btnW - 6, titleH - 6, TFT_BLACK); 
    canvas.drawRect(x + w - btnW, y, btnW, titleH, TFT_RED);     canvas.drawLine(x + w - btnW + 3, y + 3, x + w - 3, y + titleH - 3, TFT_WHITE); canvas.drawLine(x + w - 3, y + 3, x + w - btnW + 3, y + titleH - 3, TFT_WHITE); 

    for (auto el : elements) {
      int eX, eY, eW, eH;
      el->getScreenBounds(x, y + titleH, w, h - titleH, eX, eY, eW, eH);
      if (eY + eH <= y + h && eX + eW <= x + w) el->draw(eX, eY, eW, eH);
    }
    if (!isMaximized) canvas.fillTriangle(x+w-10, y+h, x+w, y+h-10, x+w, y+h, TFT_LIGHTGREY); 
  }
};

// ==========================================
// 4. DESKTOP & WINDOW MANAGER
// ==========================================
// ==========================================
// 4. DESKTOP & WINDOW MANAGER
// ==========================================
class DesktopIcon {
public:
  int x, y; String label; uint16_t color; 
  Window* targetWindow = nullptr;
  std::function<Window*()> appFactory; // The blueprint to build the app

  DesktopIcon(int _x, int _y, String _label, uint16_t _color, std::function<Window*()> _factory) 
    : x(_x), y(_y), label(_label), color(_color), appFactory(_factory) {}
  
  void draw() {
    canvas.fillRect(x, y, 32, 32, color);
    canvas.drawRect(x, y, 32, 32, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE); canvas.setTextSize(1);
    canvas.setCursor(x - 5, y + 36); canvas.print(label);
  }
  
  bool update(int mx, int my, bool clicked, std::vector<Window*>& activeWindows) {
    if (clicked && mx >= x && mx <= x + 32 && my >= y && my <= y + 32) {
      if (targetWindow == nullptr) {
        // App was fully closed. Build it fresh and allocate RAM!
        targetWindow = appFactory();
        activeWindows.push_back(targetWindow);
      } else {
        // App was just minimized. Unhide it.
        targetWindow->isOpen = true;
      }
      return true;
    }
    return false;
  }
};


class WindowManager {
public:
  std::vector<Window*> windows;
  std::vector<DesktopIcon*> icons;

  void update() {
    // --- GARBAGE COLLECTION ---
    // Clean up closed windows to free RAM
    for (int i = windows.size() - 1; i >= 0; i--) {
      if (windows[i]->isClosed) {
        Window* w = windows[i];
        windows.erase(windows.begin() + i);
        
        // Sever the link so the Desktop Icon knows it needs to rebuild next time
        for (auto icon : icons) {
          if (icon->targetWindow == w) icon->targetWindow = nullptr;
        }
        delete w; // Triggers ~Window(), freeing all UI elements from the heap!
      }
    }

    bool clicked = (mouse_left_down && !last_mouse_left_down);
    bool released = (!mouse_left_down && last_mouse_left_down);
    bool clickConsumed = false;

    // 1. Update Windows
    for (int i = windows.size() - 1; i >= 0; i--) {
      if (windows[i]->update(mouse_x, mouse_y, mouse_left_down, clicked, released)) {
        if (clicked && i != windows.size() - 1) { 
          Window* w = windows[i];
          windows.erase(windows.begin() + i);
          windows.push_back(w);
        }
        clickConsumed = true;
        break; 
      }
    }

    // 2. Update Desktop Icons
    if (!clickConsumed && clicked) {
      for (auto icon : icons) {
        // Pass the windows array so the icon can push the newly built app into it
        if (icon->update(mouse_x, mouse_y, clicked, windows)) {
          // Bring launched window to front
          for (int i = 0; i < windows.size(); i++) {
            if (windows[i] == icon->targetWindow) {
              windows.erase(windows.begin() + i);
              windows.push_back(icon->targetWindow);
              break;
            }
          }
        }
      }
    }

    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
      if (status.tab && windows.size() > 1) {
        Window* top = windows.back(); windows.pop_back(); windows.insert(windows.begin(), top);
      } else if (windows.size() > 0 && windows.back()->isOpen) {
        for (auto i : status.word) windows.back()->handleKeyboard(i);
        if (status.del) windows.back()->handleKeyboard('\b');
      }
    }

    last_mouse_left_down = mouse_left_down;

    canvas.fillScreen(TFT_DARKCYAN); 
    for (auto icon : icons) icon->draw();

    for (int i = 0; i < windows.size(); i++) {
      windows[i]->draw(i == windows.size() - 1);
    }

    canvas.fillTriangle(mouse_x, mouse_y, mouse_x+8, mouse_y+4, mouse_x+4, mouse_y+8, TFT_WHITE);
    canvas.pushSprite(0, 0);
  }
};

#endif // MICROWIN_H
