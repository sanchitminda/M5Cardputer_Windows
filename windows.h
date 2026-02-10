#include <M5Cardputer.h>

// ==========================================
// CORE LIBRARY (MicroWin Engine)
// ==========================================

// Message Codes
enum {
    WM_CREATE,
    WM_PAINT,
    WM_KEYDOWN,
    WM_TIMER,
    WM_CLOSE // New: Close command for popups
};

// Forward declaration
class Window;

// Function Pointer for Window Logic
typedef void (*WNDPROC)(Window* self, uint16_t msg, uint32_t param);

// Window Object
class Window {
public:
    int16_t x, y, w, h;     
    String title;
    WNDPROC wndProc;        
    bool hasFocus;
    uint16_t bgColor;
    
    // --- NEW: SCROLLING STATE ---
    int scrollY = 0;        // Current scroll position
    int contentHeight = 0;  // Total height of the content (text)
    
    String stateText;       

    Window(String _title, WNDPROC _proc, uint16_t _bg) {
        title = _title;
        wndProc = _proc;
        bgColor = _bg;
        hasFocus = false;
        stateText = "";     
        x = 0; y = 0; w = 100; h = 100;
        scrollY = 0;
        contentHeight = 0;
    }

    // --- NEW: SCROLLBAR DRAWER ---
    void drawScrollbar(M5Canvas* canvas) {
        // Only draw if content is taller than window
        if (contentHeight > h && h > 20) {
            int barWidth = 4;
            int trackX = x + w - barWidth;
            
            // Draw Track
            canvas->fillRect(trackX, y + 14, barWidth, h - 14, TFT_DARKGREY);
            
            // Calculate Thumb Size & Position
            // Math: (WindowH / ContentH) * WindowH
            float visibleRatio = (float)(h - 14) / (float)contentHeight;
            if (visibleRatio > 1.0) visibleRatio = 1.0;
            
            int thumbH = (int)((h - 14) * visibleRatio);
            if (thumbH < 5) thumbH = 5; // Minimum size
            
            // Math: (ScrollY / ContentH) * WindowH
            float scrollRatio = (float)scrollY / (float)contentHeight;
            int thumbY = y + 14 + (int)((h - 14) * scrollRatio);
            
            // Clamp thumb inside track
            if (thumbY + thumbH > y + h) thumbY = (y + h) - thumbH;

            // Draw Thumb
            canvas->fillRect(trackX, thumbY, barWidth, thumbH, TFT_WHITE);
        }
    }

    void drawStandardUI(M5Canvas* canvas) {
        // 1. Fill Background
        canvas->fillRect(x, y, w, h, bgColor);

        // 2. Draw Header (Title Bar)
        int headerH = 14;
        uint16_t headerColor = hasFocus ? TFT_WHITE : TFT_DARKGREY;
        uint16_t textColor   = hasFocus ? TFT_BLACK : TFT_WHITE;

        canvas->fillRect(x, y, w, headerH, headerColor);
        canvas->setTextColor(textColor);
        canvas->setTextSize(1);
        canvas->setCursor(x + 4, y + 3);
        canvas->print(title);

        // 3. Draw Scrollbar (New)
        drawScrollbar(canvas);

        // 4. Draw Border
        if (hasFocus) {
            canvas->drawRect(x, y, w, h, TFT_RED);       
        } else {
            canvas->drawRect(x, y, w, h, TFT_BLACK);
        }
    }
};

// Window Manager
class WindowManager {
private:
    std::vector<Window*> windows;
    M5Canvas* canvas;
    int focusedIndex = 0;
    
    // --- NEW: POPUP SYSTEM ---
    Window* activePopup = nullptr; // If not null, we are in "Popup Mode"

public:
    WindowManager(M5Canvas* _canvas) {
        canvas = _canvas;
    }

    void addWindow(Window* w) {
        windows.push_back(w);
        recalculateLayout();
    }

    // Function to launch a popup
    void openPopup(Window* popup) {
        activePopup = popup;
        // Center the popup
        popup->w = 160;
        popup->h = 80;
        popup->x = (240 - popup->w) / 2;
        popup->y = (135 - popup->h) / 2;
        popup->hasFocus = true;
    }

    void closePopup() {
        if (activePopup) {
            delete activePopup; // Clean up memory
            activePopup = nullptr;
        }
    }

    void recalculateLayout() {
        if (windows.empty()) return;
        int count = windows.size();
        int screenW = 240;
        int screenH = 135;
        int widthPerWin = screenW / count;

        for (int i = 0; i < count; i++) {
            windows[i]->x = i * widthPerWin;
            windows[i]->y = 0;
            windows[i]->w = widthPerWin;
            windows[i]->h = screenH;
        }
        if (count > 0) windows[focusedIndex]->hasFocus = true;
    }

    void update() {
        M5Cardputer.update();

        // --- INPUT HANDLING ---
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                // === POPUP MODE ===
                if (activePopup != nullptr) {
                    // Only send keys to popup
                     if (status.enter) {
                        // Close popup on Enter
                        closePopup();
                        return; // Skip rest of loop
                    }
                    // Pass other keys if needed
                    return; 
                }

                // === NORMAL MODE ===
                
                // 1. Global Hotkey: 'M' opens a Popup
                for (auto c : status.word) {
                    if (c == 'm' || c == 'M') {
                        // Create a simple "About" popup on the fly
                        // (Ideally we define a proc, but we can reuse a generic one)
                        // For this demo, we handle the creation in main setup mostly, 
                        // but let's signal the main loop or just flag it here.
                    }
                }

                // 2. Tab - Switch Focus
                if (status.tab) {
                    windows[focusedIndex]->hasFocus = false;
                    focusedIndex++;
                    if (focusedIndex >= windows.size()) focusedIndex = 0;
                    windows[focusedIndex]->hasFocus = true;
                }

                // 3. Pass Keys to App
                if (status.del) windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, 8);
                if (status.enter) windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, 13);
                
                // ARROW KEYS (For Scrolling)
                // The library maps arrows to specific chars sometimes, but checking 'word' is safer
                bool arrowPressed = false;
                
                // M5Cardputer library maps Arrows to:
                // UP: 180, DOWN: 181, LEFT: 182, RIGHT: 183 (approximate, changes by version)
                // Let's iterate and pass EVERYTHING to the app
                for (auto c : status.word) {
                    windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, (uint32_t)c);
                }
            }
        }

        // --- RENDER LOOP ---
        canvas->fillSprite(TFT_BLACK);
        
        // 1. Draw Normal Windows
        for (auto win : windows) {
            win->wndProc(win, WM_PAINT, 0);
        }

        // 2. Draw Popup (Overlay)
        if (activePopup != nullptr) {
            // Dim background
            // (Simulated by drawing a semi-transparent black rect, 
            // but M5GFX doesn't do alpha well on sprites easily, so we just draw over)
            
            // Draw Shadow
            canvas->fillRect(activePopup->x + 4, activePopup->y + 4, activePopup->w, activePopup->h, TFT_BLACK);
            // Draw Popup Window
            activePopup->wndProc(activePopup, WM_PAINT, 0);
        }

        canvas->pushSprite(0, 0);
    }
};