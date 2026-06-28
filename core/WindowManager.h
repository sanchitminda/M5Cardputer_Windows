#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include "hal/MouseDevice.h"
#include "hal/KeyboardDevice.h"
#include "hal/AudioDevice.h"
#include "core/FreeRTOSBridge.h"
#include "core/MicroApp.h"
#include "core/Window.h"
#include "ui/DesktopIcon.h"

// Forward declaration — PopupWindow is defined in ui/PopupWindow.h
// which is included after this file.  showPopup() is declared here
// and defined (inline) there.
class PopupWindow;

class WindowManager {
public:
  bool oomKillerEnabled = true;

  MouseDevice      mouse;
  AudioDeviceSimple audio;
  KeyboardDevice   keyboard;

  std::vector<Window*>      windows;
  std::vector<Window*>      deathRow;
  std::vector<DesktopIcon*> icons;
  std::vector<MicroApp*>    activeApps;
  Window* focusedWindow   = nullptr;
  LGFX_Sprite* wallpaperBuffer = nullptr;

  std::map<String, std::function<MicroApp*()>> appRegistry;
  std::map<String, String>                     fileAssociations;

  ~WindowManager() {
    for (auto w   : windows)    delete w;
    for (auto d   : deathRow)   delete d;
    for (auto i   : icons)      delete i;
    for (auto app : activeApps) delete app;
  }

  void begin() {
    audio.begin();
    mouse.begin();
  }

  void registerApp(String appName, std::function<MicroApp*()> factory) {
    appRegistry[appName] = factory;
  }

  void associateExt(String ext, String appName) {
    if (ext.length() > 0 && ext[0] != '.') ext = "." + ext;
    ext.toLowerCase();
    fileAssociations[ext] = appName;
  }

  void openFile(String filepath) {
    filepath.trim();
    int dot = filepath.lastIndexOf('.');
    if (dot == -1) return;
    String ext = filepath.substring(dot);
    ext.toLowerCase();
    ext.trim();
    String targetApp = "";
    for (auto const& pair : fileAssociations) {
      String reg = pair.first;
      reg.trim();
      if (reg == ext) { targetApp = pair.second; break; }
    }
    if (targetApp != "") {
      for (auto const& pair : appRegistry) {
        if (pair.first == targetApp) {
          launchApp(pair.second(), filepath);
          return;
        }
      }
    }
  }

  void spawnThread(std::function<void()> logic, const char* name = "AppThread") {
    auto* ptr = new std::function<void()>(logic);
    xTaskCreatePinnedToCore(_appTaskWrapper, name, 8192, ptr, 1, NULL, 0);
  }

  void launchApp(MicroApp* app, String args = "") {
    activeApps.push_back(app);
    app->onLaunch(args);
    if (app->window) {
      windows.push_back(app->window);
      focusedWindow = app->window;
      app->window->onClose = [this, app]() {
        app->onClose();
        this->closeWindow(app->window);
        auto it = std::find(activeApps.begin(), activeApps.end(), app);
        if (it != activeApps.end()) { activeApps.erase(it); delete app; }
      };
    }
  }

  void closeWindow(Window* win) {
    if (std::find(deathRow.begin(), deathRow.end(), win) == deathRow.end())
      deathRow.push_back(win);
  }

  void loadWallpaper(String path) {
    if (!wallpaperBuffer) {
      wallpaperBuffer = new LGFX_Sprite();
      wallpaperBuffer->setColorDepth(16);
      wallpaperBuffer->createSprite(240, 135);
    }
    if (SD.exists(path)) {
      wallpaperBuffer->drawJpgFile(SD, path.c_str(), 0, 0);
      showPopup("Wallpaper loaded", POPUP_INFO);
    } else {
      wallpaperBuffer->fillSprite(wallpaperBuffer->color565(15, 20, 25));
      for (int i = 0; i < 240; i += 20)
        wallpaperBuffer->drawFastVLine(i, 0, 135, wallpaperBuffer->color565(25, 30, 35));
      for (int i = 0; i < 135; i += 20)
        wallpaperBuffer->drawFastHLine(0, i, 240, wallpaperBuffer->color565(25, 30, 35));
      wallpaperBuffer->setTextColor(wallpaperBuffer->color565(50, 60, 70));
      wallpaperBuffer->setTextDatum(bottom_right);
      wallpaperBuffer->drawString("MicroOS v2.0", 235, 130);
      wallpaperBuffer->setTextDatum(top_left);
    }
  }

  // showPopup defined in ui/PopupWindow.h after PopupWindow is fully declared
  void showPopup(String message, PopupType type);

  void update(LGFX_Sprite* canvas) {
    keyboard.update();

    if (wallpaperBuffer)
      wallpaperBuffer->pushSprite(canvas, 0, 0);
    else
      canvas->fillScreen(TFT_CYAN);

    if (oomKillerEnabled && ESP.getFreeHeap() < 25000) {
      showPopup("KERNEL PANIC AVERTED: Critical memory threshold breached!", POPUP_ERROR);
      if (audio.isPlaying()) {
        audio.stop();
        showPopup("OOM KILLER: Forced Audio Shutdown.", POPUP_ERROR);
      }
      if (!windows.empty()) {
        Window* victim = windows.back();
        showPopup("OOM KILLER: Terminating App -> " + victim->title, POPUP_ERROR);
        closeWindow(victim);
      }
    }

    if (keyboard.currentEvent == SYS_ALT_TAB && windows.size() > 1) {
      Window* top = windows.back();
      windows.pop_back();
      windows.insert(windows.begin(), top);
      focusedWindow = windows.back();
    }

    if (focusedWindow) {
      for (char c : keyboard.typedChars) {
        if (c == KEY_ESC) { closeWindow(focusedWindow); break; }
      }
    }

    static bool prevDown = false;
    bool clicked  = (mouse.leftDown && !prevDown);
    bool released = (!mouse.leftDown && prevDown);
    prevDown = mouse.leftDown;

    // Z-order focus on click
    bool windowHit = false;
    if (clicked) {
      for (int i = windows.size() - 1; i >= 0; i--) {
        Window* w = windows[i];
        if (mouse.x >= w->x && mouse.x <= w->x + w->w &&
            mouse.y >= w->y && mouse.y <= w->y + w->h) {
          windowHit = true;
          focusedWindow = w;
          if (i != (int)windows.size() - 1) {
            windows.erase(windows.begin() + i);
            windows.push_back(w);
          }
          break;
        }
      }
    }

    // Desktop icon clicks (only when no window was hit)
    if (clicked && !windowHit) {
      for (auto icon : icons) {
        if (icon->checkClick(mouse.x, mouse.y)) {
          Window* nw = icon->onClick();
          if (nw) { windows.push_back(nw); focusedWindow = nw; }
          break;
        }
      }
    }

    // Top-down event blocking
    bool systemIsDragging = false;
    for (auto w : windows)
      if (w->isDragging || w->isResizing) { systemIsDragging = true; break; }

    bool mouseEaten = systemIsDragging;

    for (int i = windows.size() - 1; i >= 0; i--) {
      Window* w = windows[i];
      bool isActiveDragger = (w->isDragging || w->isResizing);
      bool pClick   = clicked  && (!mouseEaten || isActiveDragger);
      bool pRelease = released && (!mouseEaten || isActiveDragger);
      bool pDown    = mouse.leftDown && (!mouseEaten || isActiveDragger);
      int  pX       = (!mouseEaten || isActiveDragger) ? mouse.x : -999;
      int  pY       = (!mouseEaten || isActiveDragger) ? mouse.y : -999;
      int  pWheel   = (w == focusedWindow) ? mouse.wheel : 0;
      w->update(pX, pY, pWheel, pDown, pClick, pRelease, canvas);
      if (!mouseEaten &&
          mouse.x >= w->x && mouse.x <= w->x + w->w &&
          mouse.y >= w->y && mouse.y <= w->y + w->h)
        mouseEaten = true;
    }

    // Bottom-up rendering
    for (auto icon : icons) icon->draw(canvas);
    for (auto w    : windows) w->draw(canvas);

    // Garbage collection
    for (auto dead : deathRow) {
      auto it = std::find(windows.begin(), windows.end(), dead);
      if (it != windows.end()) windows.erase(it);
      if (focusedWindow == dead)
        focusedWindow = windows.empty() ? nullptr : windows.back();
      delete dead;
    }
    deathRow.clear();

    // Cursor
    canvas->fillTriangle(mouse.x, mouse.y, mouse.x+10, mouse.y+5, mouse.x+5, mouse.y+10, TFT_WHITE);
    canvas->drawTriangle(mouse.x, mouse.y, mouse.x+10, mouse.y+5, mouse.x+5, mouse.y+10, TFT_BLACK);

    keyboard.currentEvent = SYS_NONE;
    mouse.wheel = 0;
  }
};
