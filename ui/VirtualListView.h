#pragma once
#include <Arduino.h>
#include <deque>
#include <functional>
#include "core/UIElement.h"

struct FileEntry {
  char name[80];
  bool isDir;
};

class VirtualListView : public UIElement {
public:
  std::deque<FileEntry> items;
  int rowHeight;
  int hoveredIndex  = -1;
  int selectedIndex = -1;
  std::function<void(int index)> onItemClick;

  VirtualListView(int _x, int _y, int _w, int _rowHeight, bool _rel = false)
    : UIElement(_x, _y, _w, 0, _rel), rowHeight(_rowHeight) {}

  void addItem(String name, bool isDir) {
    FileEntry entry;
    strncpy(entry.name, name.c_str(), 79);
    entry.name[79] = '\0';
    entry.isDir = isDir;
    items.push_back(entry);
    h = items.size() * rowHeight;
  }

  void getScreenBounds(int winX, int winY, int winW, int winH,
                       int& outX, int& outY, int& outW, int& outH) override {
    UIElement::getScreenBounds(winX, winY, winW, winH, outX, outY, outW, outH);
    outH = this->h;
  }

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool clicked, bool released) override {
    bool hover = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    hoveredIndex = -1;
    if (hover) {
      int index = (my - absY) / rowHeight;
      if (index >= 0 && index < (int)items.size()) {
        hoveredIndex = index;
        if (released && onItemClick) {
          selectedIndex = index;
          onItemClick(index);
        }
      }
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    int startIdx = (absY < 0) ? (-absY) / rowHeight : 0;
    for (int i = startIdx; i < (int)items.size(); i++) {
      int itemY = absY + i * rowHeight;
      if (itemY > 135) break;
      if (itemY + rowHeight < 0) continue;
      if (i == hoveredIndex)
        canvas->fillRect(absX, itemY, elW, rowHeight, TFT_DARKGREY);
      else if (i == selectedIndex)
        canvas->fillRect(absX, itemY, elW, rowHeight, TFT_NAVY);
      canvas->setTextColor(items[i].isDir ? TFT_YELLOW : TFT_WHITE);
      canvas->setTextSize(1);
      canvas->setCursor(absX + 5, itemY + rowHeight / 2 - 3);
      if (items[i].isDir) canvas->print("[DIR] ");
      canvas->print(items[i].name);
    }
  }
};
