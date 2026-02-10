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
/////////////////////////////////////////////////////
// Window Manager
void MessageBoxProc(Window* self, uint16_t msg, uint32_t param);
class WindowManager {
private:
    std::vector<Window*> windows;
    M5Canvas* canvas;
    int focusedIndex = 0;
    
    // --- MISSING LINE ADDED HERE ---
    Window* activePopup = nullptr; 

    // State for flexibility
    bool isMaximized = false;
    float splitRatio = 0.5; 

public:
    WindowManager(M5Canvas* _canvas) {
        canvas = _canvas;
    }

    void addWindow(Window* w) {
        windows.push_back(w);
        recalculateLayout();
    }

    // --- POPUP LOGIC ---
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
            delete activePopup; // Free memory
            activePopup = nullptr;
        }
    }

    // --- LAYOUT ENGINE ---
    void recalculateLayout() {
        if (windows.empty()) return;

        int screenW = 240;
        int screenH = 135;

        // MODE 1: MAXIMIZED
        if (isMaximized) {
            for (int i = 0; i < windows.size(); i++) {
                if (i == focusedIndex) {
                    windows[i]->x = 0; 
                    windows[i]->y = 0;
                    windows[i]->w = screenW; 
                    windows[i]->h = screenH;
                } else {
                    windows[i]->w = 0; 
                    windows[i]->h = 0; // Hide others
                }
            }
            return;
        }

        // MODE 2: SPLIT VIEW (Adjustable)
        if (windows.size() == 2) {
            int splitPoint = (int)(screenW * splitRatio);
            
            // Left Window
            windows[0]->x = 0;
            windows[0]->y = 0;
            windows[0]->w = splitPoint;
            windows[0]->h = screenH;

            // Right Window
            windows[1]->x = splitPoint;
            windows[1]->y = 0;
            windows[1]->w = screenW - splitPoint;
            windows[1]->h = screenH;
            return;
        }

        // MODE 3: STANDARD TILING
        int widthPerWin = screenW / windows.size();
        for (int i = 0; i < windows.size(); i++) {
            windows[i]->x = i * widthPerWin;
            windows[i]->y = 0;
            windows[i]->w = widthPerWin;
            windows[i]->h = screenH;
        }
    }

    void update() {
        M5Cardputer.update();

        // --- INPUT HANDLING ---
        if (M5Cardputer.Keyboard.isChange()) {
            if (M5Cardputer.Keyboard.isPressed()) {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                // 1. Popup Handling (Blocks other input)
                if (activePopup != nullptr) {
                    if (status.enter) closePopup();
                    return; 
                }

                // 2. Global Hotkeys
                if (status.tab) {
                    windows[focusedIndex]->hasFocus = false;
                    focusedIndex++;
                    if (focusedIndex >= windows.size()) focusedIndex = 0;
                    windows[focusedIndex]->hasFocus = true;
                    // Force redraw if maximized to show new window
                    if (isMaximized) recalculateLayout();
                }

                if (status.opt) {
                    isMaximized = !isMaximized;
                    recalculateLayout();
                }

                // 3. RESIZING LOGIC
                bool layoutChanged = false;
                
                for (auto c : status.word) {
                    // DEBUG: Print Key Code to Serial Monitor
                    Serial.print("Key: "); Serial.println((int)c);

                    // Resize Left: fn + ';' 
                    if(status.fn){
                    if ( c == ',' ) { 
                        if (!isMaximized && windows.size() == 2) {
                            splitRatio -= 0.05; 
                            if (splitRatio < 0.2) splitRatio = 0.2;
                            layoutChanged = true;
                        }
                    }

                    // Resize Right: fn + '/'
                    if (  c == '/' ) {
                        if (!isMaximized && windows.size() == 2) {
                            splitRatio += 0.05; 
                            if (splitRatio > 0.8) splitRatio = 0.8;
                            layoutChanged = true;
                        }
                    }
                    }
                    // Open Popup Demo
                    if (c == 'm' || c == 'M') {
                         Window* popup = new Window("Alert", MessageBoxProc, TFT_LIGHTGREY);
                         openPopup(popup);
                    }
                }
                
                if (layoutChanged) recalculateLayout();

                // 4. Pass Keys to App
                if (!status.tab && !status.opt) {
                    if (status.del) windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, 8);
                    if (status.enter) windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, 13);
                    
                    for (auto c : status.word) {
                        // Don't type the resize keys into notepad
                        if (c != '<' && c != '>' && c != 180 && c != 182) {
                            windows[focusedIndex]->wndProc(windows[focusedIndex], WM_KEYDOWN, (uint32_t)c);
                        }
                    }
                }
            }
        }

        // --- RENDER ---
        canvas->fillSprite(TFT_BLACK);
        
        // Draw normal windows
        for (auto win : windows) {
            if (win->w > 0) win->wndProc(win, WM_PAINT, 0);
        }

        // Draw Popup (on top)
        if (activePopup != nullptr) {
            // Draw Shadow
            canvas->fillRect(activePopup->x + 4, activePopup->y + 4, activePopup->w, activePopup->h, TFT_BLACK);
            // Draw Popup
            activePopup->wndProc(activePopup, WM_PAINT, 0);
        }

        canvas->pushSprite(0, 0);
    }
};
