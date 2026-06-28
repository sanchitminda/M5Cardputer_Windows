#pragma once
#include "core/Window.h"
#include "core/WindowManager.h"
#include "ui/Label.h"
#include "ui/Button.h"

class PopupWindow : public Window {
public:
  PopupWindow(String message, PopupType type)
    : Window(40, 32, 160, 70, "", TFT_BLACK)
  {
    if      (type == POPUP_INFO)    { title="Info";         bgColor=TFT_NAVY;   }
    else if (type == POPUP_WARNING) { title="Warning";      bgColor=TFT_MAROON; }
    else                            { title="System Error"; bgColor=TFT_RED;    }

    addElement(new Label(5, 12, message, TFT_WHITE, false));

    Button* btnOk = new Button(60, 30, 40, 18, "OK", false);
    btnOk->onClick = [this]() { wm.closeWindow(this); };
    addElement(btnOk);
  }
};

// Out-of-line definition for WindowManager::showPopup.
// inline is required because this header is included by multiple TUs via MicroWin.h.
inline void WindowManager::showPopup(String message, PopupType type) {
  PopupWindow* popup = new PopupWindow(message, type);
  windows.push_back(popup);
  focusedWindow = popup;
}
