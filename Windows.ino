#include <SD.h>
#include <M5Cardputer.h>
#include <SPI.h>
#include "MicroWin.h"

///////////////////////////////////////////////////

// --- SYSTEM ICON DRAWING LIBRARY ---

// Helper to center the 24x24 icon in a target area (like a dock button)
void drawIconCentered(LGFX_Sprite* canvas, int tx, int ty, int tw, int th, void (*drawFunc)(LGFX_Sprite*, int, int)) {
  int ix = tx + (tw / 2) - 12; // Center a 24-pixel wide icon
  int iy = ty + (th / 2) - 12; // Center a 24-pixel high icon
  drawFunc(canvas, ix, iy);
}
// 7. SYSTEM MONITOR ICON (Microchip)
void drawMonitorIcon(LGFX_Sprite* canvas, int x, int y) {
  uint16_t chipBody = canvas->color565(30, 30, 30);
  uint16_t pinColor = TFT_SILVER;
  
  // Draw the metal pins (Top and Bottom)
  for (int i = 0; i < 4; i++) {
    canvas->fillRect(x + 5 + (i * 4), y + 3, 2, 18, pinColor);
  }
  
  // Draw the main silicon chip body
  canvas->fillRoundRect(x + 3, y + 6, 18, 12, 2, chipBody);
  
  // Draw a small bright dot in the corner (Pin 1 indicator)
  canvas->fillCircle(x + 6, y + 9, 1, TFT_CYAN);
  
  // Draw an inner square to look like the processor die
  canvas->drawRect(x + 7, y + 8, 10, 8, canvas->color565(80, 80, 80));
}
// 4. NOTES ICON (Miniature Notepad)
void drawNotesIcon(LGFX_Sprite* canvas, int x, int y) {
  // Main paper body
  canvas->fillRoundRect(x+4, y+2, 16, 20, 2, TFT_WHITE);
  
  // The clipboard clip at the top
  canvas->fillRect(x+8, y+0, 8, 4, TFT_DARKGREY); 
  
  // Subtle text lines on the paper
  canvas->drawFastHLine(x+7, y+8, 10, canvas->color565(200, 200, 200));
  canvas->drawFastHLine(x+7, y+12, 10, canvas->color565(200, 200, 200));
  canvas->drawFastHLine(x+7, y+16, 10, canvas->color565(200, 200, 200));
}

// 1. SETTINGS ICON (Stylized Sliders)
// 24x24, base Y for drawing logic
void drawSettingsIcon(LGFX_Sprite* canvas, int x, int y) {
  uint16_t c1 = TFT_LIGHTGREY;
  uint16_t c2 = TFT_WHITE;
  
  // Background circle for depth
  canvas->fillCircle(x + 12, y + 12, 11, canvas->color565(30, 30, 40));
  
  // Slider lines
  canvas->fillRect(x+6, y+8, 12, 1, c1);
  canvas->fillRect(x+6, y+16, 12, 1, c1);
  
  // Slider handle 1 (Top)
  canvas->fillRoundRect(x+6, y+6, 5, 5, 1, c2);
  // Slider handle 2 (Bottom)
  canvas->fillRoundRect(x+13, y+14, 5, 5, 1, c2);
}

// 2. DEBUGGER ICON (Code Brackets)
void drawDebugIcon(LGFX_Sprite* canvas, int x, int y) {
  // Background shield
  canvas->fillRoundRect(x+2, y+2, 20, 20, 4, TFT_YELLOW);
  
  // Brackets: '< >'
  uint16_t txtColor = TFT_BLACK;
  canvas->setTextSize(1);
  canvas->setTextColor(txtColor);
  canvas->setTextDatum(middle_center);
  // Using drawString to mathematically center the '<>' symbol
  canvas->drawString("<>", x+12, y+12);
  canvas->setTextDatum(top_left); // Reset datum!
}

// 3. NAVIDROME ICON (Musical Note)
void drawNavidromeIcon(LGFX_Sprite* canvas, int x, int y) {
  // Base Circle
  uint16_t base = canvas->color565(5, 50, 10); // Dark green
  uint16_t accent = TFT_GREEN;
  canvas->fillCircle(x+12, y+12, 11, base);
  
  // Note Head
  canvas->fillCircle(x+9, y+16, 4, accent);
  // Note Stem
  canvas->fillRect(x+12, y+6, 2, 11, accent);
  // Note Flag (Triangle)
  canvas->fillTriangle(x+14, y+6, x+20, y+8, x+14, y+11, accent);
}
// 5. FILE EXPLORER ICON (Classic Manila Folder)
void drawFolderIcon(LGFX_Sprite* canvas, int x, int y) {
  uint16_t folderBack = canvas->color565(220, 160, 0);  // Darker golden-yellow
  uint16_t folderFront = canvas->color565(255, 200, 0); // Bright golden-yellow
  
  // 1. The back tab of the folder
  canvas->fillRoundRect(x + 2, y + 4, 10, 6, 2, folderBack);
  
  // 2. A piece of white paper sticking out
  canvas->fillRect(x + 4, y + 6, 16, 10, TFT_WHITE);
  // Add a subtle line of text on the paper
  canvas->drawFastHLine(x + 6, y + 8, 8, TFT_LIGHTGREY); 
  
  // 3. The front flap of the folder
  canvas->fillRoundRect(x + 2, y + 10, 20, 12, 2, folderFront);
  
  // 4. A subtle 3D highlight on the top edge of the front flap
  canvas->drawFastHLine(x + 3, y + 10, 18, canvas->color565(255, 230, 100));
}

// 6. PAINT APP ICON (Artist's Palette & Brush)
void drawPaintIcon(LGFX_Sprite* canvas, int x, int y) {
  uint16_t wood = canvas->color565(205, 133, 63); // Warm wood color
  
  // 1. The wooden palette body
  canvas->fillCircle(x + 12, y + 14, 9, wood);
  
  // 2. The thumb hole (Black to simulate looking through it)
  canvas->fillCircle(x + 7, y + 15, 3, TFT_BLACK); 
  
  // 3. Colorful paint blobs!
  canvas->fillCircle(x + 12, y + 9, 2, TFT_RED);
  canvas->fillCircle(x + 17, y + 13, 2, TFT_GREEN);
  canvas->fillCircle(x + 15, y + 18, 2, TFT_BLUE);
  
  // 4. A paintbrush crossing the palette
  canvas->drawLine(x + 4, y + 20, x + 10, y + 14, TFT_LIGHTGREY);
  canvas->drawLine(x + 5, y + 21, x + 11, y + 15, TFT_LIGHTGREY); // Double thick handle
  canvas->fillCircle(x + 11, y + 14, 2, TFT_WHITE); // White brush tip
}
///////////////////////////////////////////////////


// --- OS KERNEL & DISPLAY ---
LGFX_Sprite canvas(&M5Cardputer.Display);
WindowManager wm;
// --- OS FLAGS ---
bool forceCacheRefresh = false;
// --- MULTITHREADING QUEUES ---
// Safely passes discovered files from the Core 0 SD Reader to the Core 1 UI
QueueHandle_t sdQueue;
// ==========================================
// APPLICATION: IMAGE VIEWER
// ==========================================
class ImageViewerApp : public MicroApp {
public:
  LGFX_Sprite* imgBuffer = nullptr;
  String currentPath = "";

  void onLaunch(String filepath) override {
    appName = "ImageViewer";
    currentPath = filepath;
    
    // Create a nearly full-screen window
    window = new Window(5, 5, 230, 125, "View: " + filepath.substring(filepath.lastIndexOf('/')+1), TFT_BLACK);

    if (filepath == "" || filepath == "/") {
      window->addElement(new Label(40, 50, "No image selected", TFT_WHITE, false));
      return;
    }

    // Allocate a buffer for the image to prevent flickering during window drags
    imgBuffer = new LGFX_Sprite();
    imgBuffer->setColorDepth(16);
    imgBuffer->createSprite(220, 100); // Internal canvas size
    imgBuffer->fillSprite(TFT_BLACK);

    // --- ASYNC LOADING ---
    wm.spawnThread([this, filepath]() {
      File f = SD.open(filepath, FILE_READ);
      if (f) {
        // Use the manual pointer method we fixed earlier to avoid abstract type errors
        // We use 'true' for the maintain-aspect-ratio scaling parameter if available
        this->imgBuffer->drawJpgFile(SD, filepath, 0, 0);
        f.close();
      }
    }, "ImageLoader");

    // Add a Custom UI Element to display the sprite
    class ImageDisplay : public UIElement {
      LGFX_Sprite** buf;
    public:
      ImageDisplay(LGFX_Sprite** b) : UIElement(5, 5, 220, 100, false), buf(b) {}
      void draw(int absX, int absY, int elW, int elH, LGFX_Sprite* canvas) override {
        if (*buf && (*buf)->getBuffer()) {
          (*buf)->pushSprite(canvas, absX, absY);
        }
      }
    };
    
    window->addElement(new ImageDisplay(&imgBuffer));
    
    // Add a simple Close button at the bottom
    Button* btnClose = new Button(85, 107, 60, 15, "Close", false);
    btnClose->onClick = [this]() { wm.closeWindow(this->window); };
    window->addElement(btnClose);
  }

  void onClose() override {
    if (imgBuffer) {
      imgBuffer->deleteSprite();
      delete imgBuffer;
    }
  }
};
// ==========================================
// APPLICATION: SYSTEM MONITOR
// ==========================================
class SystemMonitorApp : public MicroApp {
public:
  Label* lblUptime;
  Label* lblHeap;
  Label* lblSD;
  Label* lblBat;
  Label* lblWin;
  unsigned long lastUpdate = 0;

  void onLaunch(String args) override {
    appName = "SysMonitor";
    
    // Create the window
    window = new Window(20, 15, 200, 105, "Task Manager", canvas.color565(20, 25, 30));

    // Initialize all the labels with placeholder text
    lblUptime = new Label(5, 5, "Uptime: 0s", TFT_WHITE, false);
    window->addElement(lblUptime);

    lblHeap = new Label(5, 22, "RAM: Calculating...", TFT_CYAN, false);
    window->addElement(lblHeap);

    lblSD = new Label(5, 39, "SD: Calculating...", TFT_YELLOW, false);
    window->addElement(lblSD);

    lblBat = new Label(5, 56, "Battery: --%", TFT_GREEN, false);
    window->addElement(lblBat);

    lblWin = new Label(5, 73, "Open Windows: 1", TFT_LIGHTGREY, false);
    window->addElement(lblWin);

    // --- THE REAL-TIME UPDATE HOOK ---
    // The Kernel calls this 60 times a second. We throttle it to update text once per second.
    window->onUpdate = [this](Window* self) {
      if (millis() - this->lastUpdate > 1000) {
        this->refreshStats();
        this->lastUpdate = millis();
      }
    };

    // Run it once immediately so it doesn't say "Calculating..."
    refreshStats();
  }

  // Helper method to fetch hardware data and update the UI
  void refreshStats() {
    // 1. UPTIME (Convert milliseconds to HH:MM:SS)
    unsigned long secs = millis() / 1000;
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    char upStr[32];
    sprintf(upStr, "Uptime: %02dh %02dm %02ds", h, m, s);
    lblUptime->text = String(upStr);

    // 2. RAM (FreeRTOS Heap Memory)
    uint32_t freeHeap = ESP.getFreeHeap() / 1024;
    uint32_t totalHeap = ESP.getHeapSize() / 1024;
    lblHeap->text = "RAM Free: " + String(freeHeap) + "KB / " + String(totalHeap) + "KB";

    // 3. SD CARD STORAGE
    // Cast to uint32_t to avoid string conversion errors on 64-bit numbers
    uint32_t sdTotal = (uint32_t)(SD.totalBytes() / (1024 * 1024));
    uint32_t sdUsed = (uint32_t)(SD.usedBytes() / (1024 * 1024));
    lblSD->text = "SD Used: " + String(sdUsed) + "MB / " + String(sdTotal) + "MB";

    // 4. BATTERY LEVEL
    int bat = M5Cardputer.Power.getBatteryLevel();
    lblBat->text = "Battery: " + String(bat) + "%";

    // 5. WINDOW MANAGER METRICS
    lblWin->text = "Open Windows: " + String(wm.windows.size());
  }
};
// ==========================================
// 2. THE NOTEPAD APP
// ==========================================
// ==========================================
// APPLICATION: NOTEPAD
// ==========================================
class NotepadApp : public MicroApp {
public:
  // Old: TextInput* txtBox;
  MultilineTextBox* txtBox;
  Label* lblStatus;
  String currentFile; // Remember the file path so we can save it later!

  void onLaunch(String filepath) override {
    appName = "Notepad";
    currentFile = filepath;
    String title = "Untitled";
    
    // Set a default save path if we opened a blank note
    if (currentFile == "" || currentFile == "/") {
      currentFile = "/note.txt"; 
    } else {
      title = currentFile.substring(currentFile.lastIndexOf('/') + 1);
    }
    
    // 1. Build the Window (Slightly larger to fit the toolbar)
    window = new Window(15, 15, 210, 110, "Edit: " + title, canvas.color565(30, 35, 40));

    // --- 2. THE TOOLBAR (Top Row) ---
    Button* btnSave = new Button(2, 2, 45, 18, "Save", false);
    btnSave->onClick = [this]() {
      this->saveFile(); // Trigger the background save thread!
    };
    window->addElement(btnSave);

    Button* btnClear = new Button(50, 2, 45, 18, "Clear", false);
    btnClear->onClick = [this]() {
      this->txtBox->setText("");
      this->lblStatus->text = "Cleared";
      this->lblStatus->color = TFT_YELLOW;
    };
    window->addElement(btnClear);

    // Status label to show background thread progress
    lblStatus = new Label(100, 7, "Ready", TFT_LIGHTGREY, false);
    window->addElement(lblStatus);

    // --- 3. THE TEXT AREA (Bottom Row) ---
    // Pushed down to Y=22 to make room for the toolbar, with an absolute size
    // Old: txtBox = new TextInput(2, 22, 206, 86, false);
    txtBox = new MultilineTextBox(2, 22, 206, 86, false);
    window->addElement(txtBox);

    // --- 4. ASYNCHRONOUS FILE LOADING ---
    if (filepath == "" || filepath == "/") {
      txtBox->setText("Start typing here...");
      return; 
    }

    txtBox->setText("Loading...");
    lblStatus->text = "Reading...";

    // SPAWN CORE 0 THREAD: High-speed SD block reading
    wm.spawnThread([this, filepath]() {
      File f = SD.open(filepath);
      if (f) {
        size_t readLen = f.size();
        if (readLen > 16384) readLen = 16384; // 16KB limit
        
        char* buffer = new char[readLen + 1]; 
        f.read((uint8_t*)buffer, readLen);
        buffer[readLen] = '\0'; 
        
        // Push the text to the UI
        // Old: this->txtBox->text = String(buffer);
        this->txtBox->setText(String(buffer));
        this->lblStatus->text = "Loaded";
        this->lblStatus->color = TFT_GREEN;
        
        delete[] buffer; 
        f.close();
      } else {
        this->txtBox->setText("Error reading file!");
        this->lblStatus->text = "Error";
        this->lblStatus->color = TFT_RED;
      }
    }, "NotepadReader");
  }

  // --- 5. ASYNCHRONOUS FILE SAVING ---
  void saveFile() {
    lblStatus->text = "Saving...";
    lblStatus->color = TFT_CYAN;
    
    // We must copy the text and path to local variables BEFORE passing to the thread
    // Old: String dataToSave = txtBox->text;
    String dataToSave = txtBox->getText();
    String path = currentFile;

    // SPAWN CORE 0 THREAD: High-speed SD block writing
    wm.spawnThread([this, dataToSave, path]() {
      // Open with FILE_WRITE to overwrite existing content
      File f = SD.open(path, FILE_WRITE);
      if (f) {
        f.print(dataToSave);
        f.close();
        
        // Update UI safely
        this->lblStatus->text = "Saved!";
        this->lblStatus->color = TFT_GREEN;
      } else {
        this->lblStatus->text = "Save Failed!";
        this->lblStatus->color = TFT_RED;
      }
    }, "NotepadWriter");
  }

  void onClose() override {
    // The OS Garbage Collector safely destroys the Window and UI Elements!
  }
};
// ==========================================
// THE SETTINGS APP (File Associations)
// ==========================================
// ==========================================
// APPLICATION: SETTINGS
// ==========================================
class SettingsApp : public MicroApp {
public:
  TextInput* txtExt;
  TextInput* txtApp;
  Label* lblStatus;

  void onLaunch(String args) override {
    appName = "Settings";
    
    // 1. Expand the window so it fits all our settings comfortably
    window = new Window(10, 10, 220, 115, "System Settings", canvas.color565(40, 45, 50));

    // --- SECTION 1: KERNEL OPTIONS (Row 1) ---
    window->addElement(new Label(5, 5, "Kernel Options:", TFT_YELLOW, false));

    // OOM Toggle Button
    String initialText = wm.oomKillerEnabled ? "OOM: ON" : "OOM: OFF";
    Button* btnOom = new Button(5, 20, 95, 18, initialText, false);
    btnOom->onClick = [this, btnOom]() {
      wm.oomKillerEnabled = !wm.oomKillerEnabled; 
      
      if (wm.oomKillerEnabled) {
        btnOom->text = "OOM: ON";
        wm.showPopup("OOM Killer Enabled. System safe.", POPUP_INFO);
      } else {
        btnOom->text = "OOM: OFF";
        wm.showPopup("Warning: OS may crash on low RAM!", POPUP_WARNING);
      }
    };
    window->addElement(btnOom);

    // Cache Rebuild Button
    Button* btnCache = new Button(105, 20, 105, 18, "Rebuild Dir", false);
    btnCache->onClick = [this]() {
      // extern bool forceCacheRefresh; // Ensure the compiler knows this is global!
      forceCacheRefresh = true; 
      this->lblStatus->text = "Next folder will rebuild";
      this->lblStatus->color = TFT_CYAN;
    };
    window->addElement(btnCache);

    // --- SECTION 2: FILE ASSOCIATIONS (Row 2) ---
    window->addElement(new Label(5, 45, "Link File Extension:", TFT_YELLOW, false));
    
    // Extension Input (e.g., ".txt")
    txtExt = new TextInput(5, 60, 50, 16, false);
    txtExt->text = ".txt"; 
    txtExt->cursorPos = 4;
    window->addElement(txtExt);

    // Visual arrow separator
    window->addElement(new Label(60, 62, "->", TFT_WHITE, false));

    // Target App Input (e.g., "Notepad")
    txtApp = new TextInput(80, 60, 80, 16, false);
    txtApp->text = "Notepad"; 
    txtApp->cursorPos = 7;
    window->addElement(txtApp);

    // Link Action Button
    Button* btnSave = new Button(165, 60, 45, 18, "Link", false);
    btnSave->onClick = [this]() {
      String ext = this->txtExt->text;
      String app = this->txtApp->text;
      
      if (ext.length() > 0 && app.length() > 0) {
        wm.associateExt(ext, app); 
        this->lblStatus->text = "Linked " + ext + " to " + app;
        this->lblStatus->color = TFT_GREEN;
      } else {
        this->lblStatus->text = "Fields cannot be empty!";
        this->lblStatus->color = TFT_RED;
      }
    };
    window->addElement(btnSave);

    // --- SECTION 3: STATUS BAR (Row 3) ---
    lblStatus = new Label(5, 85, "Ready.", TFT_LIGHTGREY, false);
    window->addElement(lblStatus);
  }
};
// ==========================================
// 5. THE EVENT TESTER APP
// ==========================================
// ==========================================
// 5. HW & UI DEBUGGER APP
// ==========================================
// ==========================================
// 5. HW & UI DEBUGGER APP
// ==========================================
class EventTesterApp : public MicroApp {
public:
  Label* lblKey;
  Label* lblAction;

  void onLaunch(String args) override {
    appName = "HW Debugger";
    
    // Stretch the window to fit everything neatly on the Cardputer screen
    window = new Window(10, 10, 220, 115, "UI & Hardware Showcase", TFT_PURPLE);

    // --- 1. LABEL EXAMPLES (Top Row) ---
    window->addElement(new Label(5, 5, "UI Components:", TFT_YELLOW, false));
    lblKey = new Label(130, 5, "Last Key: None", TFT_CYAN, false);
    window->addElement(lblKey);

    // --- 2. TEXT INPUT EXAMPLE (Second Row) ---
    window->addElement(new Label(5, 25, "Input:", TFT_WHITE, false));
    TextInput* txtTest = new TextInput(50, 22, 160, 16, false);
    txtTest->text = "Test typing here...";
    txtTest->cursorPos = txtTest->text.length(); 
    window->addElement(txtTest);

    // --- 3. BUTTON & POPUP EXAMPLES (Third Row) ---
    lblAction = new Label(70, 45, "<- State", TFT_LIGHTGREY, false);
    
    Button* btnTest = new Button(5, 42, 60, 18, "Click Me", false);
    btnTest->onClick = [this]() {
        this->lblAction->text = "Clicked!";
        this->lblAction->color = TFT_GREEN; 
    };
    window->addElement(btnTest);
    window->addElement(lblAction);

    // THE NEW POPUP TRIGGER!
    Button* btnPopup = new Button(150, 42, 60, 18, "Popup", false);
    btnPopup->onClick = []() {
        // Summon a system-level Information popup
        wm.showPopup("UI System is stable!", POPUP_INFO);
    };
    window->addElement(btnPopup);

    // --- 4. HARDWARE AUDIO ENGINE (Bottom Rows) ---
    window->addElement(new Label(5, 65, "Audio Engine Test:", TFT_YELLOW, false));
    
    TextInput* txtAudioPath = new TextInput(5, 82, 140, 16, false);
    txtAudioPath->text = "/navakar.mp3"; 
    txtAudioPath->cursorPos = txtAudioPath->text.length();
    window->addElement(txtAudioPath);

    Button* btnPlay = new Button(150, 82, 60, 18, "Play", false);
    btnPlay->onClick = [txtAudioPath]() { 
      if (txtAudioPath->text.length() > 0) {
        wm.audio.setVolume(200);
        wm.audio.play(txtAudioPath->text);
      }
    };
    window->addElement(btnPlay);

    // --- REAL-TIME EVENT HOOK ---
    window->onUpdate = [this](Window* self) {
      if (!wm.keyboard.typedChars.empty()) {
        char c = wm.keyboard.typedChars.back();
        if (c == KEY_UP) this->lblKey->text = "Key: UP";
        else if (c == KEY_DOWN) this->lblKey->text = "Key: DOWN";
        else if (c == KEY_LEFT) this->lblKey->text = "Key: LEFT";
        else if (c == KEY_RIGHT) this->lblKey->text = "Key: RIGHT";
        else if (c == KEY_ESC) this->lblKey->text = "Key: ESC";
        else if (c == KEY_TAB) this->lblKey->text = "Key: TAB";
        else this->lblKey->text = "Key: " + String(c);
      }
    };
  }
};
// ==========================================
// 3. THE MUSIC PLAYER APP
// ==========================================
class MusicPlayerApp : public MicroApp {
public:
  Label* lblTrack;
  Button* btnPlayPause;
  uint8_t currentVol = 128; // Start at 50% volume (0-255)

  void onLaunch(String filepath) override {
    appName = "Music Player";
    String title = "No Track Loaded";
    if (filepath != "" && filepath != "/") {
      title = filepath.substring(filepath.lastIndexOf('/') + 1);
    }
    
    // Build UI
    window = new Window(20, 20, 200, 95, "Media Player", TFT_NAVY);
    
    lblTrack = new Label(10, 10, title, TFT_YELLOW, false);
    window->addElement(lblTrack);

    // Play/Pause Button
    btnPlayPause = new Button(10, 30, 60, 25, "Pause", false);
    btnPlayPause->onClick = [this]() {
      wm.audio.togglePause();
      this->btnPlayPause->text = wm.audio.isPaused ? "Play" : "Pause";
    };
    window->addElement(btnPlayPause);

    // Stop Button
    Button* btnStop = new Button(80, 30, 60, 25, "Stop", false);
    btnStop->onClick = [this]() {
      wm.audio.stop();
      this->lblTrack->text = "Stopped";
      this->btnPlayPause->text = "Play";
    };
    window->addElement(btnStop);

    // Volume Controls
    window->addElement(new Label(10, 65, "Volume:", TFT_WHITE, false));
    
    Button* btnVolDown = new Button(60, 60, 30, 20, "-", false);
    btnVolDown->onClick = [this]() {
      if (currentVol >= 15) currentVol -= 15;
      wm.audio.setVolume(currentVol);
    };
    window->addElement(btnVolDown);

    Button* btnVolUp = new Button(100, 60, 30, 20, "+", false);
    btnVolUp->onClick = [this]() {
      if (currentVol <= 240) currentVol += 15;
      wm.audio.setVolume(currentVol);
    };
    window->addElement(btnVolUp);

    // Launch the song!
    if (filepath != "" && filepath != "/") {
      wm.audio.play(filepath);
    }
    wm.audio.setVolume(currentVol);
  }

  void onClose() override {
    wm.audio.stop(); // Safe shutdown
  }
};
// ==========================================
// APPLICATION: PAINT
// ==========================================
class PaintApp : public MicroApp {
public:
  uint16_t activeColor = TFT_BLACK; // Shared state for the brush color
  PaintCanvas* pCanvas;

  void onLaunch(String args) override {
    appName = "Paint";
    
    // Create a window that fits nicely on the 240x135 screen
    window = new Window(10, 10, 220, 115, "Mini Paint", canvas.color565(230, 230, 230));

    // 1. THE TOOLBAR (Top Row)
    // We use standard buttons for our color palette
    Button* btnRed = new Button(5, 2, 20, 18, "R", false);
    btnRed->onClick = [this]() { activeColor = TFT_RED; };
    window->addElement(btnRed);

    Button* btnGreen = new Button(28, 2, 20, 18, "G", false);
    btnGreen->onClick = [this]() { activeColor = TFT_GREEN; };
    window->addElement(btnGreen);

    Button* btnBlue = new Button(51, 2, 20, 18, "B", false);
    btnBlue->onClick = [this]() { activeColor = TFT_BLUE; };
    window->addElement(btnBlue);

    Button* btnBlack = new Button(74, 2, 20, 18, "K", false);
    btnBlack->onClick = [this]() { activeColor = TFT_BLACK; };
    window->addElement(btnBlack);

    // Clear Button
    Button* btnClear = new Button(165, 2, 50, 18, "Clear", false);
    btnClear->onClick = [this]() { pCanvas->clear(); };
    window->addElement(btnClear);

    // 2. THE CANVAS (Bottom Area)
    // We pass the memory address (&activeColor) so the canvas always knows what color is selected!
    pCanvas = new PaintCanvas(5, 23, 210, 70, &activeColor);
    window->addElement(pCanvas);
  }
};
// ==========================================
// 1. THE FILE EXPLORER APP
// ==========================================
class FileExplorerApp : public MicroApp {
public:
  VirtualListView* listView;
  Label* pathLabel;
  String currentPath;
  volatile bool cancelSdTask = false;

  void onLaunch(String startingPath) override {
    appName = "File Explorer";
    currentPath = startingPath;
    
    // 1. Build the Window UI
    window = new Window(10, 10, 200, 115, "Explorer", TFT_DARKCYAN);
    
    pathLabel = new Label(5, 5, currentPath, TFT_YELLOW, false);
    window->addElement(pathLabel);

    listView = new VirtualListView(0, 20, 200, 15, false);
    window->addElement(listView);

    if (sdQueue == NULL) sdQueue = xQueueCreate(20, sizeof(FileEntry));

    // 2. Click Logic: Opening Files & Folders
    listView->onItemClick = [this](int index) {
      FileEntry entry = listView->items[index];
      String clickedName = String(entry.name);

      if (entry.isDir) {
        // Handle Folder Navigation
        if (clickedName == "..") {
          int lastSlash = currentPath.lastIndexOf('/');
          if (lastSlash > 0) currentPath = currentPath.substring(0, lastSlash);
          else currentPath = "/";
        } else {
          if (currentPath == "/") currentPath += clickedName;
          else currentPath += "/" + clickedName;
        }
        loadDirectory(currentPath);
      } 
      else {
        // Handle File Launching
        String path = currentPath;
        if (path != "/") path += "/";
        path += clickedName;

        // String ext = path.substring(path.lastIndexOf('.'));
        // ext.toLowerCase();

        wm.openFile(path);
      }
    };

    // 3. UI Update Hook: Read from the Queue safely on Core 1
    window->onUpdate = [this](Window* self) {
      FileEntry incoming;
      // Read up to 5 items per frame to keep the mouse running at 60 FPS
      for (int i = 0; i < 5; i++) {
        if (xQueueReceive(sdQueue, &incoming, 0) == pdPASS) {
          if (String(incoming.name) == "[EOF]") break; 
          this->listView->addItem(String(incoming.name), incoming.isDir);
        } else {
          break; // Queue is empty for now
        }
      }
    };

    // Kick off the initial load
    loadDirectory(currentPath);
  }

  // --- THE BACKGROUND FOLDER LOADER ---
  void loadDirectory(String path) {
    cancelSdTask = true;  
    delay(20);            // Wait for any existing SD task to safely exit
    cancelSdTask = false;
    
    listView->items.clear(); 
    listView->h = 0;
    pathLabel->text = path;

    if (path != "/") listView->addItem("..", true);

    xQueueReset(sdQueue);

    // SPAWN CORE 0 THREAD: High-Speed Cached SD Reader
    wm.spawnThread([this, path]() {
      String cachePath = path;
      if (cachePath != "/") cachePath += "/";
      cachePath += ".mwcache"; // The hidden index file

      // --- 1. FAST CACHE READ ---
      if (SD.exists(cachePath) && !forceCacheRefresh) {
        File cFile = SD.open(cachePath, FILE_READ);
        if (cFile) {
          while (cFile.available() && !this->cancelSdTask) {
            String line = cFile.readStringUntil('\n');
            line.trim();
            if (line.length() > 2) {
              FileEntry fd;
              fd.isDir = (line[0] == 'D'); // 'D' for Directory, 'F' for File
              strncpy(fd.name, line.substring(2).c_str(), 79);
              fd.name[79] = '\0';
              
              if (xQueueSend(sdQueue, &fd, pdMS_TO_TICKS(10)) == pdPASS) {
                // Yield very briefly to keep music playing perfectly
                if (wm.audio.isPlaying()) vTaskDelay(pdMS_TO_TICKS(5)); 
              }
            }
          }
          cFile.close();
          
          if (!this->cancelSdTask) {
            FileEntry eof = {"[EOF]", false};
            xQueueSend(sdQueue, &eof, portMAX_DELAY);
          }
          return; // Exit thread immediately, bypassing FAT traversal!
        }
      }

      // --- 2. SLOW DIRECTORY READ & CACHE GENERATION ---
      File dir = SD.open(path);
      if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        int count = 0;
        
        // Open a temporary file to write the index as we discover items
        String tempCache = cachePath + ".tmp";
        File tFile = SD.open(tempCache, FILE_WRITE);

        while (file && !this->cancelSdTask) {
          FileEntry fd;
          strncpy(fd.name, file.name(), 79);
          fd.name[79] = '\0';
          fd.isDir = file.isDirectory();

          // Write to the temporary cache line-by-line
          if (tFile) {
            tFile.print(fd.isDir ? "D|" : "F|");
            tFile.println(fd.name);
          }

          if (xQueueSend(sdQueue, &fd, pdMS_TO_TICKS(10)) == pdPASS) {
            file = dir.openNextFile();
            count++;
            
            // Standard dynamic throttling
            if (wm.audio.isPlaying()) vTaskDelay(pdMS_TO_TICKS(30)); 
            else if (count % 10 == 0) vTaskDelay(pdMS_TO_TICKS(1)); 
          } else {
            vTaskDelay(pdMS_TO_TICKS(5)); 
          }
        }
        dir.close();
        if (tFile) tFile.close();

        // --- 3. CACHE COMMIT ---
        if (!this->cancelSdTask) {
          if (count > 50) {
             // If massive folder, keep the cache!
             if (SD.exists(cachePath)) SD.remove(cachePath);
             SD.rename(tempCache, cachePath);
          } else {
             // If small folder, destroy the temp file to save SD space
             SD.remove(tempCache);
          }
          
          forceCacheRefresh = false; // Reset the flag after a successful rebuild
          
          FileEntry eof = {"[EOF]", false};
          xQueueSend(sdQueue, &eof, portMAX_DELAY);
        } else {
          SD.remove(tempCache); // Destroy corrupted temp file if user closed window early
        }
      }
    }, "SD_Reader");
  }

  void onClose() override {
    cancelSdTask = true; // Safely kill background loader if window is closed
  }
};

// ==========================================
// 2. MICRO WIN BOOT SEQUENCE
// ==========================================
void setup() {
  // Initialize M5Cardputer Hardware
  M5Cardputer.begin();
  M5Cardputer.Display.setRotation(1);
  canvas.createSprite(240, 135);

  // Mount SD Card
  SPI.begin(40, 39, 14, 12);
  if (!SD.begin(12, SPI, 40000000)) {
    Serial.println("SD Card Mount Failed");
  }

  // Boot the OS Hardware Abstraction Layer
  wm.begin();
  wm.begin();

  // --- 1. REGISTER APPS TO THE OS ---
  wm.registerApp("Notepad", []() { return new NotepadApp(); });
  wm.registerApp("MusicPlayer", []() { return new MusicPlayerApp(); });

  // --- 2. SET DEFAULT FILE ASSOCIATIONS ---
  wm.associateExt(".txt", "Notepad");
  wm.associateExt(".py", "Notepad");
  wm.associateExt(".mp3", "MusicPlayer");
  wm.associateExt(".wav", "MusicPlayer");
  wm.associateExt(".jpg", "ImageViewer");
  // --- 3. BUILD THE DESKTOP ---
  // ... (Your wm.icons.push_back code here) ...
  // --- BUILD THE DESKTOP ---
  
  wm.icons.push_back(new DesktopIcon(20, 80, "Notes", drawNotesIcon, []() {
    wm.launchApp(new NotepadApp(), ""); // Pass empty string for a blank note
    return nullptr; 
  }));
  // wm.icons.push_back(new DesktopIcon(20, 140, "Notes", drawNotesIcon, []() {
  //   wm.launchApp(new NotepadApp(), ""); // Pass empty string for a blank note
  //   return nullptr; 
  // }));
  // --- FILE EXPLORER ICON ---
  wm.icons.push_back(new DesktopIcon(20, 20, "Files", drawFolderIcon, []() {
    wm.launchApp(new FileExplorerApp(), "/");
    return nullptr; 
  }));

  // --- PAINT APP ICON ---
  // Assuming it sits right next to the Files app!
  wm.icons.push_back(new DesktopIcon(80, 20, "Paint", drawPaintIcon, []() {
    wm.launchApp(new PaintApp(), ""); // Launch your paint app here!
    return nullptr; 
  }));

  wm.icons.push_back(new DesktopIcon(80, 80, "Debugger", drawDebugIcon, []() {
    // EventTesterApp* app = new EventTesterApp();
    wm.launchApp(new EventTesterApp(), ""); return nullptr; 
  }));
  // Pass the drawing function (e.g., drawSettingsIcon) instead of a color
  wm.icons.push_back(new DesktopIcon(140,20 , "Settings", drawSettingsIcon, []() {
    // SettingsApp* app = new SettingsApp();
    wm.launchApp(new SettingsApp(), ""); return nullptr; 
    // return app->window;
  }));

  wm.icons.push_back(new DesktopIcon(140, 80, "SysMon", drawMonitorIcon, []() {
    // EventTesterApp* app = new EventTesterApp();
    wm.launchApp(new SystemMonitorApp(), ""); return nullptr; 
  }));
  wm.loadWallpaper("/bg.jpg");
  // wm.icons.push_back(new DesktopIcon(140, 20, "Music", drawNavidromeIcon, []() {
  //   // (We will build this app next!)
  //   return nullptr; 
  // }));
}

// ==========================================
// 3. MAIN KERNEL LOOP
// ==========================================
void loop() {
  // 1. Keep the USB hardware ticking (since MouseDevice inherited EspUsbHost)
  wm.mouse.task(); 
  
  // 2. Process all OS logic, routing, UI, and Garbage Collection
  wm.update(&canvas); 
  
  // 3. Push the rendered frame to the physical screen
  canvas.pushSprite(0, 0); 
}
