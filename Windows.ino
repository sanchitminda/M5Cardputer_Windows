#include <Arduino.h>
#include "MicroWin.h"
#include <M5Cardputer.h>


WindowManager wm;

// ==========================================
// 5. MAIN SETUP
// ==========================================
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  canvas.createSprite(240, 135);
  canvas.setColorDepth(16);

  Serial.begin(115200);
  delay(500);
  usbHost.begin();

  // --- APP 1: Paint Factory ---
  auto createPaint = []() -> Window* {
    Window* win = new Window(30, 20, 140, 90, "Paint", TFT_LIGHTGREY);
    win->addElement(new PaintCanvas(5, 5, 90, 90, true)); 
    return win;
  };
  wm.icons.push_back(new DesktopIcon(10, 10, "Paint", TFT_RED, createPaint));

  // --- APP 2: Notepad Factory ---
  auto createNote = []() -> Window* {
    Window* win = new Window(60, 40, 150, 80, "Notepad", TFT_WHITE);
    win->addElement(new TextInput(0, 0, 100, 100, true)); 
    return win;
  };
  wm.icons.push_back(new DesktopIcon(10, 60, "Notepad", TFT_BLUE, createNote));

  // --- APP 3: System Monitor Factory ---
  auto createStats = []() -> Window* {
    Window* win = new Window(100, 10, 110, 90, "SysMonitor", TFT_BLACK);
    Label* lblMem = new Label(5, 10, "RAM:", TFT_GREEN, false);
    Label* lblTime = new Label(5, 30, "Uptime:", TFT_GREEN, false);
    Label* lblBat = new Label(5, 50, "Battery:", TFT_GREEN, false);
    win->addElement(lblMem); win->addElement(lblTime);win->addElement(lblBat);
    win->onUpdate = [lblMem, lblTime,lblBat](Window* self) {
      lblMem->text = "RAM: " + String(ESP.getFreeHeap() / 1024) + " KB";
      lblTime->text = "Up: " + String(millis() / 1000) + "s";
      lblBat->text = "Bat: " + String(M5Cardputer.Power.getBatteryLevel()) + "%";
    };
    return win;
  };
  wm.icons.push_back(new DesktopIcon(60, 10, "SysMon", TFT_DARKGREEN, createStats));
}

void loop() {
  usbHost.task();
  wm.update();
}