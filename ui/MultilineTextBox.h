#pragma once
#include <Arduino.h>
#include <vector>
#include "core/UIElement.h"
#include "core/WindowManager.h"   // needs wm.keyboard.typedChars

class MultilineTextBox : public UIElement {
public:
  std::vector<String> lines;
  int  cursorRow  = 0;
  int  cursorCol  = 0;
  int  topRow     = 0;
  int  lineHeight = 12;
  bool isFocused  = false;
  bool isHovered  = false;

  MultilineTextBox(int _x, int _y, int _w, int _h, bool _rel = false)
    : UIElement(_x, _y, _w, _h, _rel) {
    lines.push_back("");
  }

  void setText(String t) {
    lines.clear();
    int start = 0, end = t.indexOf('\n');
    while (end != -1) {
      String line = t.substring(start, end);
      line.replace("\r", "");
      lines.push_back(line);
      start = end + 1;
      end   = t.indexOf('\n', start);
    }
    String last = t.substring(start);
    last.replace("\r", "");
    lines.push_back(last);
    cursorRow = 0; cursorCol = 0; topRow = 0;
  }

  String getText() {
    String res = "";
    for (size_t i = 0; i < lines.size(); i++) {
      res += lines[i];
      if (i < lines.size() - 1) res += "\n";
    }
    return res;
  }

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX+elW && my >= absY && my <= absY+elH);
    if (clicked) isFocused = isHovered;

    if (isFocused && !wm.keyboard.typedChars.empty()) {
      for (char c : wm.keyboard.typedChars) {
        if (c == KEY_UP) {
          if (cursorRow > 0) cursorRow--;
          if (cursorCol > (int)lines[cursorRow].length()) cursorCol = lines[cursorRow].length();
        } else if (c == KEY_DOWN) {
          if (cursorRow < (int)lines.size()-1) cursorRow++;
          if (cursorCol > (int)lines[cursorRow].length()) cursorCol = lines[cursorRow].length();
        } else if (c == KEY_LEFT) {
          if (cursorCol > 0) cursorCol--;
          else if (cursorRow > 0) { cursorRow--; cursorCol = lines[cursorRow].length(); }
        } else if (c == KEY_RIGHT) {
          if (cursorCol < (int)lines[cursorRow].length()) cursorCol++;
          else if (cursorRow < (int)lines.size()-1) { cursorRow++; cursorCol = 0; }
        } else if (c == '\b' || c == 8 || c == 127) {
          if (cursorCol > 0) {
            lines[cursorRow].remove(cursorCol-1, 1); cursorCol--;
          } else if (cursorRow > 0) {
            String cur = lines[cursorRow];
            lines.erase(lines.begin() + cursorRow);
            cursorRow--;
            cursorCol = lines[cursorRow].length();
            lines[cursorRow] += cur;
          }
        } else if (c == '\n' || c == '\r') {
          String right = lines[cursorRow].substring(cursorCol);
          lines[cursorRow] = lines[cursorRow].substring(0, cursorCol);
          lines.insert(lines.begin() + cursorRow + 1, right);
          cursorRow++; cursorCol = 0;
        } else if (c >= 32 && c <= 126) {
          lines[cursorRow] = lines[cursorRow].substring(0, cursorCol)
                           + String(c)
                           + lines[cursorRow].substring(cursorCol);
          cursorCol++;
        }
      }
      wm.keyboard.typedChars.clear();

      int maxVis = (elH - 8) / lineHeight;
      if (cursorRow < topRow) topRow = cursorRow;
      if (cursorRow >= topRow + maxVis) topRow = cursorRow - maxVis + 1;
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    uint16_t bg     = canvas->color565(20, 22, 28);
    uint16_t border = isFocused ? TFT_CYAN : canvas->color565(80, 80, 80);
    canvas->fillRoundRect(absX, absY, elW, elH, 3, bg);
    canvas->drawRoundRect(absX, absY, elW, elH, 3, border);
    if (!isFocused)
      canvas->drawFastHLine(absX+2, absY+1, elW-4, canvas->color565(10,10,10));

    canvas->setClipRect(absX+2, absY+2, elW-4, elH-4);
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextDatum(top_left);
    canvas->setTextSize(1);

    int maxVis = (elH - 8) / lineHeight;
    for (int i = topRow; i < (int)lines.size() && i <= topRow+maxVis; i++) {
      int ty = absY + 4 + (i-topRow) * lineHeight;
      canvas->drawString(lines[i], absX+4, ty);
      if (isFocused && i == cursorRow && (millis() % 1000 < 500)) {
        String up = lines[i].substring(0, cursorCol);
        int cx = absX + 4 + canvas->textWidth(up);
        canvas->drawFastVLine(cx, ty, 8, TFT_CYAN);
      }
    }
    canvas->clearClipRect();
  }
};
