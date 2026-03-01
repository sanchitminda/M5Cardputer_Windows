#include "lgfx/v1/misc/enum.hpp"
#ifndef MICROWIN_H
#define MICROWIN_H

#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include <deque>
#include <functional>
#include <algorithm>
#include <class/hid/hid.h>
#include "EspUsbHost.h"
// --- NEW: OS Hardware Libraries ---
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutput.h>
#include <AudioFileSourceID3.h> // WE NEED THIS BACK!
#include <map> // NEW: For the App Registry


// We use non-printable ASCII control codes so they don't render as gibberish on the screen!
const char KEY_UP    = 0x11; 
const char KEY_DOWN  = 0x12; 
const char KEY_LEFT  = 0x13; 
const char KEY_RIGHT = 0x14;
const char KEY_ESC   = 0x1B; // Standard ASCII Escape code (27)
enum SystemEvent {
  SYS_NONE,
  SYS_ALT_TAB,
  SYS_CTRL_ALT_DEL
};
// --- POPUP SEVERITY LEVELS ---
enum PopupType {
  POPUP_INFO,
  POPUP_WARNING,
  POPUP_ERROR
};
// class PopupWindow ;
// ==========================================
// OS HARDWARE ABSTRACTION LAYER (HAL)
// ==========================================

// By inheriting from EspUsbHost, the mouse driver handles its own USB interrupts!
class MouseDevice : public EspUsbHost {
public:
  int x = 120; // Start at center of screen
  int y = 67;
  int wheel = 0;
  bool leftDown = false;

  void begin() {
    EspUsbHost::begin();
    EspUsbHost::setHIDLocal(HID_LOCAL_US);
  }

  // Overriding the hardware callbacks
  void onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons) override {
    leftDown = (report.buttons & MOUSE_BUTTON_LEFT);
  }

  void onMouseMove(hid_mouse_report_t report) override {
    x += (int8_t)report.x;
    y += (int8_t)report.y;
    wheel = (int8_t)report.wheel;

    // Constrain to M5Cardputer 240x135 screen natively in the OS
    if (x < 0) x = 0;
    if (x > 239) x = 239;
    if (y < 0) y = 0;
    if (y > 134) y = 134;
  }
};

class AudioOutputM5Speaker : public AudioOutput {
  public:
    AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0) {
      _m5sound = m5sound;
      _virtual_ch = virtual_sound_channel;
    }
    virtual ~AudioOutputM5Speaker(void) {};
    virtual bool begin(void) override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override {
      if (_tri_buffer_index < tri_buf_size) {
        _tri_buffer[_tri_index][_tri_buffer_index  ] = sample[0];
        _tri_buffer[_tri_index][_tri_buffer_index+1] = sample[1];
        _tri_buffer_index += 2;
        return true;
      }
      flush();
      return false;
    }
    virtual void flush(void) override {
      if (_tri_buffer_index) {
        _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
        _tri_index = _tri_index < 2 ? _tri_index + 1 : 0;
        _tri_buffer_index = 0;
      }
    }
    virtual bool stop(void) override {
      flush();
      _m5sound->stop(_virtual_ch);
      return true;
    }
    const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
  protected:
    m5::Speaker_Class* _m5sound;
    uint8_t _virtual_ch;
    static constexpr size_t tri_buf_size = 640;
    int16_t _tri_buffer[3][tri_buf_size];
    size_t _tri_buffer_index = 0;
    size_t _tri_index = 0;
};
//////////////////////////
// --- THE BARE-MINIMUM AUDIO ENGINE ---
// --- THE BARE-MINIMUM AUDIO ENGINE (V2) ---
class AudioDeviceSimple {
// --- THE MULTI-THREADED AUDIO ENGINE (Core 0 - SPI Safe) ---
private:
  AudioGeneratorMP3 *mp3 = nullptr;
  AudioFileSourceSD *file = nullptr;
  AudioFileSourceID3 *id3 = nullptr; 
  AudioFileSourceBuffer *buff = nullptr; // BRINGING THE BUCKET BACK!
  AudioOutputM5Speaker *out = nullptr; 
  TaskHandle_t taskHandle = nullptr;

  char nextSong[100] = "";
  volatile bool triggerPlay = false;
  volatile bool triggerStop = false;

  void cleanup() {
    if (mp3 && mp3->isRunning()) mp3->stop();
    if (buff) { buff->close(); delete buff; buff = nullptr; }
    if (id3) { id3->close(); delete id3; id3 = nullptr; }
    if (file) { file->close(); delete file; file = nullptr; }
    isPaused = false;
  }

  static void _audioTask(void *parameter) {
    AudioDeviceSimple *audio = (AudioDeviceSimple*)parameter;
    for (;;) {
      
      if (audio->triggerPlay) {
        audio->triggerPlay = false;
        audio->cleanup(); 
        
        audio->file = new AudioFileSourceSD(audio->nextSong);
        if (audio->file->isOpen()) {
          audio->id3 = new AudioFileSourceID3(audio->file);
          
          // The 4KB bucket! Core 0 fills this while Core 1 is sleeping.
          audio->buff = new AudioFileSourceBuffer(audio->id3, 4096); 
          
          if (audio->mp3->begin(audio->buff, audio->out)) {
            audio->isPaused = false;
          } else {
            audio->cleanup();
          }
        } else {
          audio->cleanup();
        }
      }

      if (audio->triggerStop) {
        audio->triggerStop = false;
        audio->cleanup();
      }

      if (audio->mp3 && audio->mp3->isRunning()) {
        if (!audio->isPaused) {
          if (!audio->mp3->loop()) {
            audio->cleanup(); 
          }
          // Pet the watchdog with a tiny 1ms sleep so the core doesn't panic
          vTaskDelay(pdMS_TO_TICKS(1)); 
        } else {
          vTaskDelay(pdMS_TO_TICKS(20)); 
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(50)); 
      }
    }
  }

public:
  volatile bool isPaused = false;

  void begin() {
    out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);
    mp3 = new AudioGeneratorMP3();
    xTaskCreatePinnedToCore(_audioTask, "AudioTask", 32768, this, 2, &taskHandle, 0);
  }

  void play(String path) {
    path.trim();
    strncpy(nextSong, path.c_str(), 99);
    nextSong[99] = '\0';
    triggerPlay = true; 
  }

  void stop() { triggerStop = true; }
  void togglePause() { if (isPlaying()) isPaused = !isPaused; }
  void setVolume(uint8_t vol) { M5Cardputer.Speaker.setVolume(vol); }
  bool isPlaying() { return (mp3 && mp3->isRunning()); }
};
///////////////////////////
// --- THE OS AUDIO SUBSYSTEM ---
// class AudioDevice {
// private:
//   AudioGeneratorMP3 *mp3 = nullptr;
//   AudioFileSourceSD *file = nullptr;
//   AudioFileSourceBuffer *buff = nullptr;
//   AudioFileSourceID3 *id3 = nullptr; //
//   AudioOutputM5Speaker *out = nullptr; 
//   TaskHandle_t taskHandle = nullptr;

//   // Internal Mailbox
//   volatile bool triggerPlay = false;
//   volatile bool triggerStop = false;
//   volatile bool triggerPause = false; // NEW
//   char nextSong[100] = "";

//   static void _audioDriverTask(void *parameter) {
//     AudioDevice *audio = (AudioDevice*)parameter;
//     for (;;) {
      
//       // --- 1. START PLAYBACK COMMAND ---
//       if (audio->triggerPlay) {
        
//         // A. TEARDOWN OLD MEMORY (Strict Outside-In Order!)
//         if (audio->mp3 && audio->mp3->isRunning()) audio->mp3->stop();
//         if (audio->buff) { audio->buff->close(); delete audio->buff; audio->buff = nullptr; }
//         if (audio->id3) { audio->id3->close(); delete audio->id3; audio->id3 = nullptr; }
//         if (audio->file) { audio->file->close(); delete audio->file; audio->file = nullptr; }

//         audio->triggerPlay = false;

//         // B. OPEN THE NEW FILE
//         audio->file = new AudioFileSourceSD(audio->nextSong);
        
//         // C. VERIFY FILE IS OPEN BEFORE CREATING WRAPPERS
//         if (audio->file->isOpen()) { 
          
//           // Wrap in ID3 to filter out Album Art
//           audio->id3 = new AudioFileSourceID3(audio->file);
          
//           // Wrap in 4KB Buffer (4096 is safer for ESP32 RAM limits!)
//           audio->buff = new AudioFileSourceBuffer(audio->id3, 4096); 
          
//           // Boot the MP3 Engine
//           if (audio->mp3->begin(audio->buff, audio->out)) {
//             audio->isPaused = false;
//           } else {
//             Serial.println("MP3 Decoder rejected the file!");
//             if (audio->mp3->isRunning()) audio->mp3->stop();
//           }
//         } else {
//           Serial.println("SD Card could not open the file!");
//           // Clean up the dead file object so it doesn't leak!
//           delete audio->file; 
//           audio->file = nullptr;
//         }
//       }

//       // --- 2. STOP PLAYBACK COMMAND ---
//       if (audio->triggerStop) {
//         if (audio->mp3 && audio->mp3->isRunning()) audio->mp3->stop();
//         if (audio->buff) { audio->buff->close(); delete audio->buff; audio->buff = nullptr; }
//         if (audio->id3) { audio->id3->close(); delete audio->id3; audio->id3 = nullptr; }
//         if (audio->file) { audio->file->close(); delete audio->file; audio->file = nullptr; }
//         audio->triggerStop = false;
//         audio->isPaused = false;
//       }

//       // --- 3. PAUSE COMMAND ---
//       if (audio->triggerPause) {
//         audio->isPaused = !audio->isPaused;
//         audio->triggerPause = false;
//       }

//       // --- 4. HIGH-SPEED AUDIO MATH LOOP ---
//       if (audio->mp3 && audio->mp3->isRunning()) {
//         if (!audio->isPaused) {
          
//           // Decode the next tiny chunk of audio
//           if (!audio->mp3->loop()) {
//              audio->mp3->stop(); // Song naturally finished
//           }
          
//           // CRITICAL FIX: Use taskYIELD() here to prevent I2S DMA Underflow!
//           // Do NOT use vTaskDelay(1) while playing music!
//           taskYIELD(); 
          
//         } else {
//           vTaskDelay(pdMS_TO_TICKS(10)); // Sleep deeply if paused
//         }
//       } else {
//         vTaskDelay(pdMS_TO_TICKS(10)); // Sleep deeply if idle
//       }
//     }
//   }

// public:
//   volatile bool isPaused = false;

//   void begin() {
//     out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);
//     mp3 = new AudioGeneratorMP3();
//     xTaskCreatePinnedToCore(_audioDriverTask, "AudioDriver", 32768, this, 2, &taskHandle, 0);
//   }


//   void play(String path) {
//     path.trim(); // NEW: Strip invisible \r or \n characters!
//     strncpy(nextSong, path.c_str(), 99);
//     nextSong[99] = '\0';
//     triggerPlay = true;
//   }

//   void stop() { triggerStop = true; }
  
//   // NEW PUBLIC METHODS
//   void togglePause() { 
//     if (isPlaying()) triggerPause = true; 
//   }
  
//   void setVolume(uint8_t vol) { 
//     M5Cardputer.Speaker.setVolume(vol); // Harness M5Unified's native volume mapping!
//   }

//   bool isPlaying() { return (mp3 && mp3->isRunning()); }
// };


class KeyboardDevice {
public:
  std::vector<char> typedChars;
  bool backspace = false;
  bool enter = false;
// Modifier Tracking
  bool fnPressed = false;
  bool ctrlPressed = false;
  bool altPressed = false;
  bool tabPressed = false;
  SystemEvent currentEvent = SYS_NONE;

  // --- CUSTOM FN KEY MAPPING ---
  // The Cardputer handles basic symbols automatically, but you can 
  // override or add custom macros here when Fn is held down!
  // --- CUSTOM FN KEY MAPPING ---
  char applyFnMap(char c) {
    switch (c) {
      case ';': return KEY_UP;
      case '.': return KEY_DOWN;
      case ',': return KEY_LEFT;
      case '/': return KEY_RIGHT;
      case '`': return KEY_ESC; // Keep your custom symbols!
      default: return c;
    }
  }
  void update() {
    typedChars.clear();
    backspace = false;
    enter = false;

    // Read the M5Cardputer's physical I2C keyboard
    M5Cardputer.update();
    
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();
      // Read Modifiers directly from M5Unified
      fnPressed = status.fn;
      ctrlPressed = status.ctrl;
      altPressed = status.alt;
      tabPressed = status.tab;
      // if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB)) status.tab = true;
      if (status.del) backspace = true;
      if (status.enter) enter = true;

      // --- HOTKEY INTERCEPTION ---
      // Ctrl + Alt + Delete
      if (ctrlPressed && altPressed && status.del) {
        currentEvent = SYS_CTRL_ALT_DEL;
        return; // Return immediately to block normal typing!
      }
      
      // Alt + Tab
      if (altPressed && tabPressed) {
        // We only trigger once per press, not continuously while held
        // if (M5Cardputer.Keyboard.isChange()) {
           currentEvent = SYS_ALT_TAB;
        // }
        return; 
      }

      // --- NORMAL TYPING & FN TRANSLATION ---
      if (M5Cardputer.Keyboard.isPressed()) {
        for (auto c : status.word) {
          if (fnPressed) {
            c = applyFnMap(c); // Apply your custom mapping table!
          }
          typedChars.push_back(c);
        }
      }
      if (status.del) backspace = true;
      if (status.enter) enter = true;
    }
  }
};
// ==========================================
// FORWARD DECLARATIONS
// ==========================================
class Window;
class WindowManager;
// ==========================================
// 1. FREE RTOS C++ BRIDGE
// ==========================================
static void _appTaskWrapper(void* parameter) {
  auto* taskLogic = static_cast<std::function<void()>*>(parameter);
  (*taskLogic)(); 
  delete taskLogic; 
  vTaskDelete(NULL); 
}

// ==========================================
// 2. THE BASE APPLICATION CLASS (SDK)
// ==========================================
class MicroApp {
public:
  Window* window = nullptr;
  String appName = "Unknown App";

  virtual ~MicroApp() {}

  // Mandatory method: What happens when the app starts?
  virtual void onLaunch(String args) = 0;

  // Optional method: What happens when the app is closed?
  virtual void onClose() {} 
};

// ==========================================
// 3. UI ELEMENT BASE & COMPONENTS
// ==========================================
class UIElement {
public:
  int x, y, w, h;
  bool isRelative;

  UIElement(int _x, int _y, int _w, int _h, bool _rel = false) 
    : x(_x), y(_y), w(_w), h(_h), isRelative(_rel) {}
  virtual ~UIElement() {}

  virtual void getScreenBounds(int winX, int winY, int winW, int winH, int &outX, int &outY, int &outW, int &outH) {
    outX = winX + (isRelative ? (winW * x / 100) : x);
    outY = winY + y;
    outW = isRelative ? (winW * w / 100) : w;
    outH = h;
  }

  virtual void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) {}
  virtual void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) = 0;
};

class Label : public UIElement {
public:
  String text;
  uint16_t color;
  Label(int _x, int _y, String _text, uint16_t _color, bool _rel = false) 
    : UIElement(_x, _y, 0, 10, _rel), text(_text), color(_color) {}

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    canvas->setTextColor(color);
    canvas->setTextSize(1);
    canvas->setCursor(absX, absY);
    canvas->print(text);
  }
};

class Button : public UIElement {
public:
  String text;
  std::function<void()> onClick;
  bool isHovered = false;
  bool isPressed = false; // NEW: Tracks the physical "pushed down" state!

  Button(int _x, int _y, int _w, int _h, String _text, bool _rel = false) 
    : UIElement(_x, _y, _w, _h, _rel), text(_text) {}

  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    
    // Check if the user is actively holding the mouse down on this button
    isPressed = (isHovered && leftDown);

    if (isHovered && released && onClick) {
      onClick();
    }
  }

  // --- 3D BUTTON RENDERER ---
  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    
    if (isPressed) {
      // --- PRESSED STATE ---
      // The button physically moves +2 pixels down and right.
      // Notice: We DO NOT draw the shadow here, because the button is pushed into it!
      int pushX = absX + 2;
      int pushY = absY + 2;

      canvas->fillRoundRect(pushX, pushY, elW, elH, 3, canvas->color565(100, 100, 100)); // Darker when pressed
      canvas->drawRoundRect(pushX, pushY, elW, elH, 3, TFT_DARKGREY);
      
      canvas->setTextColor(TFT_WHITE);
      canvas->setTextDatum(middle_center);
      canvas->drawString(text, pushX + (elW / 2), pushY + (elH / 2));
      canvas->setTextDatum(top_left); // Always reset alignment!
      
    } else {
      // --- NORMAL / HOVER STATE ---
      
      // 1. DRAW THE DROP SHADOW FIRST (Offset by +2 pixels)
      // Using a very dark grey to simulate a cast shadow on the UI
      canvas->fillRoundRect(absX + 2, absY + 2, elW, elH, 3, canvas->color565(30, 30, 30));

      // 2. DRAW THE BUTTON BODY ON TOP
      uint16_t bgColor = isHovered ? TFT_LIGHTGREY : TFT_DARKGREY;
      canvas->fillRoundRect(absX, absY, elW, elH, 3, bgColor);
      canvas->drawRoundRect(absX, absY, elW, elH, 3, TFT_WHITE);
      
      // 3. DRAW THE TEXT
      canvas->setTextColor(TFT_WHITE);
      canvas->setTextDatum(middle_center);
      canvas->drawString(text, absX + (elW / 2), absY + (elH / 2));
      canvas->setTextDatum(top_left); 
    }
  }
};


// ==========================================
// 4. VIRTUAL LIST VIEW (Optimized Recycler)
// ==========================================
struct FileEntry {
  char name[80]; 
  bool isDir;
};

class VirtualListView : public UIElement {
public:
  std::deque<FileEntry> items;
  int rowHeight;
  int hoveredIndex = -1;
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

  void getScreenBounds(int winX, int winY, int winW, int winH, int &outX, int &outY, int &outW, int &outH) override {
    UIElement::getScreenBounds(winX, winY, winW, winH, outX, outY, outW, outH);
    outH = this->h; 
  }

  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    bool isHoveredBounds = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    hoveredIndex = -1;
    
    if (isHoveredBounds) {
      int relY = my - absY;
      int index = relY / rowHeight;
      if (index >= 0 && index < items.size()) {
        hoveredIndex = index;
        if (released && onItemClick) {
          selectedIndex = index;
          onItemClick(index);
        }
      }
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    int startIdx = 0;
    if (absY < 0) {
      startIdx = (-absY) / rowHeight; 
    }

    for (int i = startIdx; i < items.size(); i++) {
      int itemY = absY + (i * rowHeight);

      if (itemY > 135) break; 
      if (itemY + rowHeight < 0) continue; 

      if (i == hoveredIndex) {
        canvas->fillRect(absX, itemY, elW, rowHeight, TFT_DARKGREY);
      } else if (i == selectedIndex) {
        canvas->fillRect(absX, itemY, elW, rowHeight, TFT_NAVY);
      }

      canvas->setTextColor(items[i].isDir ? TFT_YELLOW : TFT_WHITE);
      canvas->setTextSize(1);
      canvas->setCursor(absX + 5, itemY + (rowHeight / 2) - 3);
      
      if (items[i].isDir) {
        canvas->print("[DIR] ");
      }
      canvas->print(items[i].name);
    }
  }
};

// ==========================================
// 5. WINDOW CLASS
// ==========================================
extern WindowManager wm; // Allows Window to access the Garbage Collector

class Window {
public:
  int x, y, w, h;
  String title;
  uint16_t bgColor;
  std::vector<UIElement*> elements;
  QueueHandle_t ipcQueue;
  
  int scrollY = 0;
  int contentHeight = 0;
  // --- WINDOW PHYSICS STATES ---
  bool isDragging = false;
  bool isResizing = false;
  int dragOffsetX = 0;
  int dragOffsetY = 0;

  bool isMinimized = false;
  int normalH = 0; 

  bool isMaximized = false;
  int normX, normY, normW, normH;
  
  std::function<void()> deferredTask = nullptr;
  std::function<void(Window*)> onUpdate = nullptr;
  std::function<void()> onClose = nullptr;
  

Window(int _x, int _y, int _w, int _h, String _title, uint16_t _bg) {
    x = _x; y = _y; w = _w; h = _h; title = _title; bgColor = _bg;
    
    // Create a hardware-safe mailbox that can hold ten 64-character messages!
    ipcQueue = xQueueCreate(10, sizeof(char[64])); 
  }

  ~Window() {
    if (onClose) onClose();
    for (auto el : elements) delete el;
    elements.clear();
    if (ipcQueue != nullptr) {
      vQueueDelete(ipcQueue);
    }
  }

  void clearElements() {
    for (auto el : elements) delete el;
    elements.clear();
    scrollY = 0;
    contentHeight = 0;
  }

  void addElement(UIElement* el) {
    elements.push_back(el);
  }

  void computeContentHeight() {
    int maxH = h - 15; 
    for (auto el : elements) {
      int eX, eY, eW, eH;
      el->getScreenBounds(0, 0, w, h - 15, eX, eY, eW, eH); 
      if (eY + eH + 15 > maxH) {
        maxH = eY + eH + 15; 
      }
    }
    contentHeight = maxH;
  }

  void update(int mx, int my, int wheel, bool leftDown, bool clicked, bool released, LGFX_Sprite* canvas);
  void draw(LGFX_Sprite* canvas);
};

// ==========================================
// 6. DESKTOP ICON
// ==========================================
class DesktopIcon {
public:
  int x, y;
  String name;
  
  // NEW: Holds the custom drawing function instead of a flat color!
  void (*iconFunc)(LGFX_Sprite*, int, int) = nullptr;
  
  std::function<Window*()> onClick;

  DesktopIcon(int _x, int _y, String _name, void (*_iconFunc)(LGFX_Sprite*, int, int), std::function<Window*()> _onClick)
    : x(_x), y(_y), name(_name), iconFunc(_iconFunc), onClick(_onClick) {}

  bool checkClick(int mx, int my) {
    // The clickable hit-box remains 32x32 pixels
    return (mx >= x && mx <= x + 32 && my >= y && my <= y + 32);
  }

  void draw(LGFX_Sprite* canvas) {
    // 1. Draw the Vector Icon
    if (iconFunc != nullptr) {
      // The hitbox is 32x32, but our icons are 24x24.
      // We add +4 to X and Y to perfectly center the icon inside the clickable area!
      iconFunc(canvas, x + 4, y + 4);
    }

    // 2. Draw the App Name (Mathematically Centered)
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    
    // Tell LovyanGFX to anchor the text at the top-center
    canvas->setTextDatum(top_center);
    
    // Draw the text exactly in the middle of the 32px icon (x + 16), 
    // and place it 34 pixels down so it sits just below the icon.
    canvas->drawString(name, x + 16, y + 34);
    
    // Always reset the alignment so other UI elements don't break!
    canvas->setTextDatum(top_left); 
  }
};

// ==========================================
// 7. WINDOW MANAGER (OS KERNEL)
// ==========================================
class WindowManager {
public:
// --- OS HARDWARE SUBSYSTEMS ---
bool oomKillerEnabled = true;
  MouseDevice mouse;
  AudioDeviceSimple audio;
  KeyboardDevice keyboard;
  std::vector<Window*> windows;
  std::vector<Window*> deathRow;   
  std::vector<DesktopIcon*> icons;
  std::vector<MicroApp*> activeApps;
  Window* focusedWindow = nullptr; 
  // NEW: Dedicated memory for the desktop wallpaper
  LGFX_Sprite* wallpaperBuffer = nullptr; 

  // --- WALLPAPER ENGINE ---
  void loadWallpaper(String path) {
    // 1. Allocate a 240x135 memory buffer for the image
    if (wallpaperBuffer == nullptr) {
      // Assuming 'canvas' is globally available, or pass your main display
      wallpaperBuffer = new LGFX_Sprite(); 
      wallpaperBuffer->setColorDepth(16);
      wallpaperBuffer->createSprite(240, 135);
    }
    // showPopup("Loading wallpaper", POPUP_INFO);
    // 2. Try to load the JPG from the SD Card directly into RAM
    if (SD.exists(path)) {
      // LovyanGFX has native JPG decoding!
      wallpaperBuffer->drawJpgFile(SD, path.c_str(), 0, 0);
      showPopup("Loading wallpaper", POPUP_INFO);
    } else {
      // 3. FALLBACK: Procedural Blueprint Grid
      wallpaperBuffer->fillSprite(wallpaperBuffer->color565(15, 20, 25)); // Deep slate
      
      // Draw grid lines
      for (int i = 0; i < 240; i += 20) {
        wallpaperBuffer->drawFastVLine(i, 0, 135, wallpaperBuffer->color565(25, 30, 35));
      }
      for (int i = 0; i < 135; i += 20) {
        wallpaperBuffer->drawFastHLine(0, i, 240, wallpaperBuffer->color565(25, 30, 35));
      }
      
      // Add a cool OS watermark in the corner
      wallpaperBuffer->setTextColor(wallpaperBuffer->color565(50, 60, 70));
      wallpaperBuffer->setTextDatum(bottom_right);
      wallpaperBuffer->drawString("MicroOS v1.0", 235, 130);
      wallpaperBuffer->setTextDatum(top_left); // Reset alignment
    }
  }
// --- THE APP REGISTRY ---
  // Maps "App Name" -> Factory Function that creates the App
  std::map<String, std::function<MicroApp*()>> appRegistry;
  // Maps ".extension" -> "App Name"
  std::map<String, String> fileAssociations;
  ~WindowManager() {
    for (auto win : windows) delete win;
    for (auto dead : deathRow) delete dead;
    for (auto icon : icons) delete icon;
    for (auto app : activeApps) delete app;
  }
  void registerApp(String appName, std::function<MicroApp*()> factory) {
    appRegistry[appName] = factory;
  }
  void showPopup(String message, PopupType type);
  void associateExt(String ext, String appName) {
    if (ext.length() > 0 && ext[0] != '.') ext = "." + ext; 
    ext.toLowerCase();
    fileAssociations[ext] = appName;
  }

  // --- BULLETPROOF FILE ROUTER ---
  void openFile(String filepath) {
    filepath.trim(); // 1. Strip invisible SD card characters!
    
    int dotIndex = filepath.lastIndexOf('.');
    if (dotIndex == -1) return; // No extension

    String ext = filepath.substring(dotIndex);
    ext.toLowerCase();
    ext.trim(); // 2. Clean the extracted extension

    // 3. Resilient Lookup (Bypasses std::map binary sorting bugs)
    String targetApp = "";
    for (auto const& pair : fileAssociations) {
      String registeredExt = pair.first;
      registeredExt.trim();
      if (registeredExt == ext) {
        targetApp = pair.second;
        break;
      }
    }

    // 4. Instantiate and Launch the App
    if (targetApp != "") {
      for (auto const& pair : appRegistry) {
        if (pair.first == targetApp) {
          MicroApp* newApp = pair.second();
          launchApp(newApp, filepath);
          return;
        }
      }
    }
  }
  void spawnThread(std::function<void()> customLogic, const char* taskName = "AppThread") {
    auto* logicPtr = new std::function<void()>(customLogic);
    xTaskCreatePinnedToCore(_appTaskWrapper, taskName, 8192, logicPtr, 1, NULL, 0);
  }

  void launchApp(MicroApp* app, String args = "") {
    activeApps.push_back(app);
    app->onLaunch(args); 
    
    if (app->window) {
      this->windows.push_back(app->window);
      this->focusedWindow = app->window;
      
      app->window->onClose = [this, app]() {
        app->onClose(); 
        this->closeWindow(app->window); 
        
        auto it = std::find(activeApps.begin(), activeApps.end(), app);
        if (it != activeApps.end()) {
          activeApps.erase(it);
          delete app; 
        }
      };
    }
  }
  // --- SYSTEM POPUP SPAWNER ---
  // void showPopup(String message, PopupType type) {
  //   PopupWindow* popup = new PopupWindow(message, type);
    
  //   // Force it to the top of the window stack
  //   windows.push_back(popup);
  //   focusedWindow = popup;
  // }

  void closeWindow(Window* win) {
    if (std::find(deathRow.begin(), deathRow.end(), win) == deathRow.end()) {
      deathRow.push_back(win);
    }
  }
  // --- KERNEL BOOT SEQUENCE ---
  void begin() {
    audio.begin();
    mouse.begin();
  }

  // --- KERNEL UPDATE LOOP ---
  void update(LGFX_Sprite* canvas) {
    // 1. HARDWARE READS
    keyboard.update(); 
    // --- 2. GLOBAL HOTKEY EVENT HANDLER ---
    // --- 2. THE KERNEL OOM KILLER ---
    // The ESP32 needs roughly 20KB of free heap just to run its background Wi-Fi and Bluetooth tasks.
    // If we drop below 25,000 bytes, we are in the Danger Zone!
    // LAYER 1: The Desktop Background
    if (wallpaperBuffer != nullptr) {
      // Instantly stamp the pre-loaded image onto the main canvas (Zero SD card reads!)
      wallpaperBuffer->pushSprite(canvas, 0, 0);
    } else {
      canvas->fillScreen(TFT_CYAN); // Failsafe
    }
    if (oomKillerEnabled && ESP.getFreeHeap() < 25000) {
      // Serial.println();
      wm.showPopup("KERNEL PANIC AVERTED: Critical memory threshold breached!", POPUP_ERROR);
      // Step 1: Nuke the audio buffer (which takes a lot of RAM)
      if (audio.isPlaying()) {
        audio.stop();

      wm.showPopup("OOM KILLER: Forced Audio Shutdown.", POPUP_ERROR);

      }

      // Step 2: Ruthlessly assassinate the active application to free memory
      if (!windows.empty()) {
        Window* victim = windows.back(); 
        wm.showPopup("OOM KILLER: Terminating App -> "+victim->title, POPUP_ERROR);

        closeWindow(victim);
      }
    }

    if (keyboard.currentEvent == SYS_CTRL_ALT_DEL) {
       // Hard Reboot the OS!
       // (Alternatively, you could launch your SystemMonitorApp here instead)
      //  ESP.restart(); 
    }
    else if (keyboard.currentEvent == SYS_ALT_TAB) {
       // Cycle the Windows!
       if (windows.size() > 1) {
         // Grab the top window and shove it to the absolute bottom of the Z-order
         Window* top = windows.back();
         windows.pop_back();
         windows.insert(windows.begin(), top);
         
         // Set focus to the new top window!
         focusedWindow = windows.back(); 
       }
    }
    if (focusedWindow != nullptr) {
      for (char c : keyboard.typedChars) {
        if (c == KEY_ESC) {
          closeWindow(focusedWindow);
          break; // Stop processing characters so we don't crash reading a dead window!
        }
      }
    }
    
    static bool prevDown = false;
    bool clicked = (mouse.leftDown && !prevDown);
    bool released = (!mouse.leftDown && prevDown);
    prevDown = mouse.leftDown;

    // 2. Z-ORDER & FOCUS
    bool windowHit = false;
    if (clicked) {
      for (int i = windows.size() - 1; i >= 0; i--) {
        Window* w = windows[i];
        if (mouse.x >= w->x && mouse.x <= w->x + w->w && mouse.y >= w->y && mouse.y <= w->y + w->h) {
          windowHit = true;
          focusedWindow = w;
          if (i != windows.size() - 1) {
            windows.erase(windows.begin() + i);
            windows.push_back(w);
          }
          break; 
        }
      }
    }

    // 3. DESKTOP ICONS (Only trigger if no window was clicked)
    if (clicked && !windowHit) {
      for (auto icon : icons) {
        if (icon->checkClick(mouse.x, mouse.y)) {
           Window* newWin = icon->onClick();
           if (newWin) {
             windows.push_back(newWin);
             focusedWindow = newWin; 
           }
           break;
        }
      }
    }

    // --- 4. TOP-DOWN EVENT BLOCKING ---
    // First, check if the user is actively dragging/resizing a window
    bool systemIsDragging = false;
    for (auto w : windows) {
       if (w->isDragging || w->isResizing) { systemIsDragging = true; break; }
    }

    bool mouseEaten = systemIsDragging; // If dragging, NO ONE else gets the mouse!

    for (int i = windows.size() - 1; i >= 0; i--) {
      Window* w = windows[i];
      
      // The actively dragging window always bypasses the blocker
      bool isTheActiveDragger = (w->isDragging || w->isResizing);
      
      // Mask the inputs if a higher window already ate the mouse
      bool pClick   = clicked && (!mouseEaten || isTheActiveDragger);
      bool pRelease = released && (!mouseEaten || isTheActiveDragger);
      bool pDown    = mouse.leftDown && (!mouseEaten || isTheActiveDragger);
      
      // Pass fake coordinates (-999) so buttons underneath don't light up!
      int pX = (!mouseEaten || isTheActiveDragger) ? mouse.x : -999;
      int pY = (!mouseEaten || isTheActiveDragger) ? mouse.y : -999;
      int pWheel = (w == focusedWindow) ? mouse.wheel : 0;
      
      w->update(pX, pY, pWheel, pDown, pClick, pRelease, canvas);

      // Once we process the window under the mouse, we EAT the mouse for the windows below!
      if (!mouseEaten && (mouse.x >= w->x && mouse.x <= w->x + w->w && mouse.y >= w->y && mouse.y <= w->y + w->h)) {
         mouseEaten = true;
      }
    }

    // --- 5. BOTTOM-UP RENDERING ---
    // canvas->fillSprite(TFT_BLACK); 
    for (auto icon : icons) icon->draw(canvas);

    for (auto w : windows) {
      w->draw(canvas);
    }

    // 6. GARBAGE COLLECTION
    for (auto deadWin : deathRow) {
      auto it = std::find(windows.begin(), windows.end(), deadWin);
      if (it != windows.end()) {
        windows.erase(it);
      }
      if (focusedWindow == deadWin) {
        focusedWindow = windows.empty() ? nullptr : windows.back();
      }
      delete deadWin; 
    }
    deathRow.clear(); 

    // 7. RENDER CURSOR
    canvas->fillTriangle(mouse.x, mouse.y, mouse.x + 10, mouse.y + 5, mouse.x + 5, mouse.y + 10, TFT_WHITE);
    canvas->drawTriangle(mouse.x, mouse.y, mouse.x + 10, mouse.y + 5, mouse.x + 5, mouse.y + 10, TFT_BLACK);
    
    keyboard.currentEvent = SYS_NONE;
    // audio.loop();
    mouse.wheel = 0; 
  }
  
};

// ==========================================
// 8. WINDOW METHODS IMPLEMENTATION
// ==========================================
// Implemented down here so WindowManager is fully defined for wm.closeWindow()
void Window::update(int mx, int my, int wheel, bool leftDown, bool clicked, bool released, LGFX_Sprite* canvas) {
  if (onUpdate) onUpdate(this);

  int titleH = 18; // Sleek, slightly taller title bar

  // 1. CLEAR PHYSICS STATES
  if (released) {
    isDragging = false;
    isResizing = false;
  }

  // 2. MAC-STYLE TITLE BAR BUTTONS
  if (clicked) {
    // [X] Close (Rightmost, Red)
    if (mx >= x + w - 20 && mx <= x + w - 4 && my >= y && my <= y + titleH) {
      wm.closeWindow(this);
      return;
    }
    // [O] Maximize (Middle, Green)
    if (mx >= x + w - 36 && mx < x + w - 20 && my >= y && my <= y + titleH) {
      if (isMaximized) {
        x = normX; y = normY; w = normW; h = normH; 
        isMaximized = false;
      } else {
        normX = x; normY = y; normW = w; normH = h; 
        x = 0; y = 0; w = 240; h = 135; 
        isMaximized = true;
        isMinimized = false; 
      }
      return;
    }
    // [-] Minimize (Leftmost, Yellow)
    if (mx >= x + w - 52 && mx < x + w - 36 && my >= y && my <= y + titleH) {
      if (isMinimized) {
        h = normalH; 
        isMinimized = false;
      } else {
        normalH = h; 
        h = titleH; // Roll up to exactly the title bar height!
        isMinimized = true;
      }
      return;
    }
  }

  // 3. DRAGGING (Grab the title bar, excluding the buttons!)
  if (clicked && !isMaximized && my >= y && my < y + titleH && mx >= x && mx < x + w - 52) {
    isDragging = true;
    dragOffsetX = mx - x;
    dragOffsetY = my - y;
  }
  
  if (isDragging && leftDown) {
    x = mx - dragOffsetX;
    y = my - dragOffsetY;
    if (y < 0) y = 0; 
  }

  // 4. RESIZING (Grab bottom-right corner)
  if (clicked && !isMaximized && !isMinimized && 
      mx >= x + w - 15 && mx <= x + w && my >= y + h - 15 && my <= y + h) {
    isResizing = true;
  }
  
  if (isResizing && leftDown) {
    w = mx - x;
    h = my - y;
    if (w < 80) w = 80; 
    if (h < 40) h = 40; 
  }

  // 5. UPDATE UI ELEMENTS
  if (isMinimized) return; 

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
    el->getScreenBounds(x, y + titleH - scrollY, w, h - titleH, eX, eY, eW, eH);
    el->update(eX, eY, eW, eH, mx, my, leftDown, clicked, released);
  }
}

void Window::draw(LGFX_Sprite* canvas) {
  int titleH = 18;

  // 1. DROP SHADOW
  // Draws a soft dark layer under the window, offset to the bottom right
  if (!isMaximized) {
    canvas->fillRoundRect(x + 4, y + 4, w, h, 6, canvas->color565(15, 15, 15));
  }

  // 2. MAIN BACKGROUND
  canvas->fillRoundRect(x, y, w, h, 6, bgColor);
  
  // 3. TITLE BAR
  uint16_t titleColor = canvas->color565(40, 45, 55); // Sleek Slate Grey
  canvas->fillRoundRect(x, y, w, titleH, 6, titleColor);
  if (!isMinimized) {
    canvas->fillRect(x, y + 10, w, titleH - 10, titleColor); // Merge into window body
  }
  
  // 4. BORDERS (Subtle 3D Highlight)
  canvas->drawRoundRect(x, y, w, h, 6, canvas->color565(90, 90, 90));
  canvas->drawFastHLine(x, y + titleH, w, canvas->color565(60, 60, 60)); // Separator line
  
  // 5. TITLE TEXT
  canvas->setTextColor(TFT_WHITE);
  canvas->setTextSize(1);
  canvas->setTextDatum(middle_left); // Mathematically center text vertically!
  canvas->drawString(title, x + 8, y + (titleH / 2));
  canvas->setTextDatum(top_left); // Reset alignment
  
  // 6. MAC-STYLE WINDOW BUTTONS (Traffic Lights)
  // [X] Close (Red)
  canvas->fillCircle(x + w - 12, y + (titleH / 2), 5, canvas->color565(255, 96, 92));
  // [O] Maximize (Green)
  canvas->fillCircle(x + w - 28, y + (titleH / 2), 5, canvas->color565(38, 201, 64));
  // [-] Minimize (Yellow)
  canvas->fillCircle(x + w - 44, y + (titleH / 2), 5, canvas->color565(255, 189, 46));

  if (isMinimized) return; 

  // 7. HARDWARE CLIPPING
  canvas->setClipRect(x + 1, y + titleH + 1, w - 2, h - titleH - 2);

  // 8. DRAW ELEMENTS
  for (auto el : elements) {
    int eX, eY, eW, eH;
    el->getScreenBounds(x, y + titleH - scrollY, w, h - titleH, eX, eY, eW, eH);
    if (eY + eH > y + titleH && eY < y + h) {
      el->draw(eX, eY, eW, eH, canvas);
    }
  }
  canvas->clearClipRect(); 

  // 9. MODERN ROUNDED SCROLLBAR
  computeContentHeight();
  int maxScroll = contentHeight - (h - titleH);
  if (maxScroll > 0) {
    int thumbH = max((h - titleH) * (h - titleH) / contentHeight, 15);
    int thumbY = y + titleH + 2 + (scrollY * (h - titleH - thumbH - 4) / maxScroll);
    
    // Draw a dark pill-shaped track
    canvas->fillRoundRect(x + w - 6, y + titleH + 2, 4, h - titleH - 4, 2, canvas->color565(30, 30, 30));
    // Draw a light pill-shaped thumb on top
    canvas->fillRoundRect(x + w - 6, thumbY, 4, thumbH, 2, canvas->color565(150, 150, 150));
  }

  // 10. SLEEK RESIZE GRABBER (Diagonal Lines)
  if (!isMaximized) {
    uint16_t grabColor = canvas->color565(120, 120, 120);
    canvas->drawLine(x + w - 4, y + h - 10, x + w - 10, y + h - 4, grabColor);
    canvas->drawLine(x + w - 4, y + h - 6, x + w - 6, y + h - 4, grabColor);
  }

  if (deferredTask) {
    deferredTask();
    deferredTask = nullptr;
  }
}



extern WindowManager wm; // Promising the compiler that 'wm' exists in the .ino file!
class TextInput : public UIElement {
public:
  String text;
  bool isHovered = false;
  bool isFocused = false; // Tracks if the user clicked inside!
  int cursorPos = 0; // NEW: Tracks the index of the cursor!
  
  TextInput(int _x, int _y, int _w, int _h, bool _rel = false) 
    : UIElement(_x, _y, _w, _h, _rel) {}

  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    // If the user clicks the mouse, gain focus if we are hovered, lose it if we aren't!
    if (clicked) {
      isFocused = isHovered;
      if (isFocused) cursorPos = text.length();
    }
    if (cursorPos > text.length()) cursorPos = text.length();
    if (cursorPos < 0) cursorPos = 0;

    // Only listen to the keyboard if this specific text box is focused
    if (isFocused) {
      // Handle Backspace
      if (wm.keyboard.backspace && text.length() > 0) {
        text.remove(text.length() - 1);
        cursorPos--;
      }
      // Append any newly typed characters
      // --- 2. HANDLE TYPING & NAVIGATION ---
      for (char c : wm.keyboard.typedChars) {
        if (c == KEY_LEFT) {
          if (cursorPos > 0) cursorPos--;
          continue;
        }
        if (c == KEY_RIGHT) {
          if (cursorPos < text.length()) cursorPos++;
          continue;
        }
        if (c == KEY_UP || c == KEY_DOWN || c == KEY_ESC) {
          continue; // Ignore these for now
        }

        // Splice the string and insert the new character at the cursor!
        String leftPart = text.substring(0, cursorPos);
        String rightPart = text.substring(cursorPos);
        text = leftPart + c + rightPart;
        cursorPos++;
      }
    }
  }
  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas);
  // void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
  //   canvas->fillRect(absX, absY, elW, elH, TFT_BLACK);
  //   canvas->drawRect(absX, absY, elW, elH, isHovered ? TFT_YELLOW : TFT_WHITE);
  //   canvas->setTextColor(TFT_WHITE);
  //   canvas->setTextSize(1);
  //   canvas->setCursor(absX + 2, absY + 2);
  //   canvas->print(text);
  // }
};


// --- MODERN TEXT INPUT RENDERER ---
void TextInput::draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas){
  
  // 1. DETERMINE COLORS BASED ON STATE
  // A very dark, sleek grey/blue for the recessed background
  uint16_t bgColor = canvas->color565(20, 22, 28); 
  uint16_t borderColor = canvas->color565(80, 80, 80); // Default sleek grey border
  
  if (isFocused) {
    borderColor = TFT_CYAN; // Bright glowing focus ring
  } else if (isHovered) {
    borderColor = TFT_LIGHTGREY; // Subtle highlight when mouse passes over
  }

  // 2. DRAW THE MAIN RECESSED BOX
  canvas->fillRoundRect(absX, absY, elW, elH, 3, bgColor);
  canvas->drawRoundRect(absX, absY, elW, elH, 3, borderColor);

  // 3. INNER 3D SHADOW (Only when not focused)
  // Drawing a dark line along the top inner edge makes it look "pushed in" to the window!
  if (!isFocused) {
    canvas->drawFastHLine(absX + 2, absY + 1, elW - 4, canvas->color565(10, 10, 10));
  } else {
    // Add a double-thick border to simulate a glowing focus ring
    canvas->drawRoundRect(absX + 1, absY + 1, elW - 2, elH - 2, 2, canvas->color565(0, 100, 150));
  }

  // 4. DRAW THE TEXT (Mathematically Centered!)
  canvas->setTextColor(TFT_WHITE);
  canvas->setTextDatum(middle_left); 
  // Offset X by +4 for padding, center Y exactly in the middle of the box
  canvas->drawString(text, absX + 4, absY + (elH / 2));
  canvas->setTextDatum(top_left); // Always reset alignment for the rest of the OS
  
  // 5. RENDER THE HARDWARE CURSOR
  if (isFocused && (millis() % 1000 < 500)) {
    // Calculate exactly how many pixels wide the text is up to the cursor index
    String textUpToCursor = text.substring(0, cursorPos);
    int cursorPixelX = absX + 4 + canvas->textWidth(textUpToCursor); // +4 to match text padding
    
    // Draw a sleek vertical line, perfectly centered with the text
    int cursorH = 10; // Taller, elegant cursor
    int cursorY = absY + (elH / 2) - (cursorH / 2);
    
    // Cursor color matches the glowing border!
    canvas->drawFastVLine(cursorPixelX, cursorY, cursorH, TFT_CYAN);
  }
}
// ==========================================
// 6. SYSTEM POPUP IMPLEMENTATION
// ==========================================

// Now the compiler knows what a Window and a Button are!
class PopupWindow : public Window {
public:
  PopupWindow(String message, PopupType type) 
    : Window(40, 32, 160, 70, "", TFT_BLACK) 
  {
    if (type == POPUP_INFO) {
      title = "Info";
      bgColor = TFT_NAVY;        
    } else if (type == POPUP_WARNING) {
      title = "Warning";
      bgColor = TFT_MAROON;      
    } else if (type == POPUP_ERROR) {
      title = "System Error";
      bgColor = TFT_RED;         
    }

    addElement(new Label(5, 12, message, TFT_WHITE, false));

    Button* btnOk = new Button(60, 30, 40, 18, "OK", false);
    
    // The compiler now knows exactly what 'wm' is down here!
    btnOk->onClick = [this]() {
      wm.closeWindow(this); 
    };
    
    addElement(btnOk);
  }
};

// Now we provide the code for that WindowManager method we promised earlier!
inline void WindowManager::showPopup(String message, PopupType type) {
  PopupWindow* popup = new PopupWindow(message, type);
  windows.push_back(popup);
  focusedWindow = popup;
}

// ==========================================
// CUSTOM UI ELEMENT: PAINT CANVAS
// ==========================================
class PaintCanvas : public UIElement {
public:
  LGFX_Sprite* buffer;
  uint16_t* currentColor;
  int lastX = -1;
  int lastY = -1;

  PaintCanvas(int _x, int _y, int _w, int _h, uint16_t* _colorRef) 
    : UIElement(_x, _y, _w, _h, false), currentColor(_colorRef) 
  {
    // Allocate a dedicated memory buffer for the artwork
    buffer = new LGFX_Sprite();
    buffer->setColorDepth(16); // High color mode
    buffer->createSprite(_w, _h);
    buffer->fillSprite(TFT_WHITE); // Start with a white canvas
  }

  ~PaintCanvas() {
    // Crucial: Free the memory when the window is closed to prevent OOM panics!
    buffer->deleteSprite();
    delete buffer;
  }

  void clear() {
    buffer->fillSprite(TFT_WHITE);
  }

  // --- DRAWING PHYSICS ---
  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    bool isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    
    if (isHovered && leftDown) {
      // Calculate where the mouse is relative to the canvas
      int localX = mx - absX;
      int localY = my - absY;
      
      if (lastX != -1 && lastY != -1) {
        // Draw a thick line connecting the previous frame's mouse position to the current one.
        // This ensures smooth curves even if the mouse moves fast!
        buffer->drawLine(lastX, lastY, localX, localY, *currentColor);
        buffer->drawLine(lastX + 1, lastY, localX + 1, localY, *currentColor); // 2px thick
        buffer->drawLine(lastX, lastY + 1, localX, localY + 1, *currentColor);
      } else {
        // Just started clicking, draw a dot
        buffer->fillCircle(localX, localY, 1, *currentColor);
      }
      lastX = localX;
      lastY = localY;
    } else {
      // Lifted the brush
      lastX = -1;
      lastY = -1;
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    // Stamp our persistent memory buffer onto the main OS window!
    buffer->pushSprite(canvas, absX, absY);
    
    // Draw a nice recessed 3D border around the canvas
    canvas->drawFastHLine(absX, absY, elW, canvas->color565(100, 100, 100));
    canvas->drawFastVLine(absX, absY, elH, canvas->color565(100, 100, 100));
    canvas->drawRect(absX, absY, elW, elH, TFT_DARKGREY);
  }
};
// ==========================================
// CUSTOM UI ELEMENT: MULTILINE TEXT BOX
// ==========================================
class MultilineTextBox : public UIElement {
public:
  std::vector<String> lines;
  int cursorRow = 0;
  int cursorCol = 0;
  int topRow = 0; // Tracks vertical scrolling
  int lineHeight = 12; // 8px text + 4px padding
  bool isFocused = false;
  bool isHovered = false;

  MultilineTextBox(int _x, int _y, int _w, int _h, bool _rel = false) 
    : UIElement(_x, _y, _w, _h, _rel) {
    lines.push_back(""); // Start with one empty line
  }

  // Helper to load a massive string into our 2D vector
  void setText(String t) {
    lines.clear();
    int start = 0;
    int end = t.indexOf('\n');
    while (end != -1) {
      // Remove pesky carriage returns from Windows files
      String line = t.substring(start, end);
      line.replace("\r", ""); 
      lines.push_back(line);
      start = end + 1;
      end = t.indexOf('\n', start);
    }
    String lastLine = t.substring(start);
    lastLine.replace("\r", "");
    lines.push_back(lastLine);
    cursorRow = 0; cursorCol = 0; topRow = 0;
  }

  // Helper to flatten our 2D vector back into a single string for saving
  String getText() {
    String res = "";
    for (size_t i = 0; i < lines.size(); i++) {
      res += lines[i];
      if (i < lines.size() - 1) res += "\n";
    }
    return res;
  }

  void update(int absX, int absY, int elW, int elH, int mx, int my, bool leftDown, bool clicked, bool released) override {
    isHovered = (mx >= absX && mx <= absX + elW && my >= absY && my <= absY + elH);
    if (clicked) isFocused = isHovered;

    // --- 2D KEYBOARD ENGINE ---
    if (isFocused && !wm.keyboard.typedChars.empty()) {
      for (char c : wm.keyboard.typedChars) {
        
        if (c == KEY_UP) {
          if (cursorRow > 0) cursorRow--;
          if (cursorCol > lines[cursorRow].length()) cursorCol = lines[cursorRow].length();
        
        } else if (c == KEY_DOWN) {
          if (cursorRow < lines.size() - 1) cursorRow++;
          if (cursorCol > lines[cursorRow].length()) cursorCol = lines[cursorRow].length();
        
        } else if (c == KEY_LEFT) {
          if (cursorCol > 0) cursorCol--;
          else if (cursorRow > 0) { // Wrap to previous line
            cursorRow--; 
            cursorCol = lines[cursorRow].length(); 
          }
        
        } else if (c == KEY_RIGHT) {
          if (cursorCol < lines[cursorRow].length()) cursorCol++;
          else if (cursorRow < lines.size() - 1) { // Wrap to next line
            cursorRow++; 
            cursorCol = 0; 
          }
        
        } else if (c == '\b' || c == 8 || c == 127) { // Backspace
          if (cursorCol > 0) {
            lines[cursorRow].remove(cursorCol - 1, 1);
            cursorCol--;
          } else if (cursorRow > 0) {
            // Merge current line with the previous line
            String currentLine = lines[cursorRow];
            lines.erase(lines.begin() + cursorRow);
            cursorRow--;
            cursorCol = lines[cursorRow].length();
            lines[cursorRow] += currentLine;
          }
        
        } else if (c == '\n' || c == '\r') { // Enter/Return
          // Split the current line in two!
          String rightPart = lines[cursorRow].substring(cursorCol);
          lines[cursorRow] = lines[cursorRow].substring(0, cursorCol);
          lines.insert(lines.begin() + cursorRow + 1, rightPart);
          cursorRow++;
          cursorCol = 0;
        
        } else if (c >= 32 && c <= 126) { // Standard printable characters
          lines[cursorRow] = lines[cursorRow].substring(0, cursorCol) + String(c) + lines[cursorRow].substring(cursorCol);
          cursorCol++;
        }
      }
      wm.keyboard.typedChars.clear(); // Consume the hardware buffer!

      // --- AUTOMATIC SCROLL CAMERA ---
      // Keeps the cursor in view if you type past the bottom or arrow-key past the top
      int maxVisibleRows = (elH - 8) / lineHeight;
      if (cursorRow < topRow) topRow = cursorRow;
      if (cursorRow >= topRow + maxVisibleRows) topRow = cursorRow - maxVisibleRows + 1;
    }
  }

  void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
    // 1. Sleek Recessed Background
    uint16_t bgColor = canvas->color565(20, 22, 28); 
    uint16_t borderColor = isFocused ? TFT_CYAN : canvas->color565(80, 80, 80); 
    canvas->fillRoundRect(absX, absY, elW, elH, 3, bgColor);
    canvas->drawRoundRect(absX, absY, elW, elH, 3, borderColor);
    if (!isFocused) canvas->drawFastHLine(absX + 2, absY + 1, elW - 4, canvas->color565(10, 10, 10));

    // 2. Hardware Clipping Ring (Ensures text never bleeds outside the box!)
    canvas->setClipRect(absX + 2, absY + 2, elW - 4, elH - 4);

    canvas->setTextColor(TFT_WHITE);
    canvas->setTextDatum(top_left);
    canvas->setTextSize(1);
    
    // 3. Render only the visible rows
    int maxVisibleRows = (elH - 8) / lineHeight;
    for (int i = topRow; i < lines.size() && i <= topRow + maxVisibleRows; i++) {
      int textY = absY + 4 + ((i - topRow) * lineHeight);
      canvas->drawString(lines[i], absX + 4, textY);
      
      // 4. Render the Blinking Cursor on the active line
      if (isFocused && i == cursorRow && (millis() % 1000 < 500)) {
        String upToCursor = lines[i].substring(0, cursorCol);
        int cursorX = absX + 4 + canvas->textWidth(upToCursor);
        canvas->drawFastVLine(cursorX, textY, 8, TFT_CYAN);
      }
    }
    canvas->clearClipRect(); // Always clear the clip rect so the rest of the OS can draw!
  }
};
#endif
