#include "windows.h"

// ==========================================
//  USER APPLICATIONS 
// ==========================================

M5Canvas* globalCanvas; 
WindowManager* wm;

// --- APP: POPUP MESSAGE ---
void MessageBoxProc(Window* self, uint16_t msg, uint32_t param) {
    if (msg == WM_PAINT) {
        // Draw Box
        globalCanvas->fillRect(self->x, self->y, self->w, self->h, TFT_LIGHTGREY);
        globalCanvas->drawRect(self->x, self->y, self->w, self->h, TFT_WHITE);
        
        // Draw Header
        globalCanvas->fillRect(self->x, self->y, self->w, 15, TFT_RED);
        globalCanvas->setTextColor(TFT_WHITE);
        globalCanvas->setCursor(self->x + 5, self->y + 3);
        globalCanvas->print("System Alert");

        // Draw Message
        globalCanvas->setTextColor(TFT_BLACK);
        globalCanvas->setCursor(self->x + 10, self->y + 30);
        globalCanvas->print("Hello User!");
        globalCanvas->setCursor(self->x + 10, self->y + 45);
        globalCanvas->print("Press ENTER to close");
    }
}

// --- APP: NOTEPAD (With Scrolling) ---
void NotepadProc(Window* self, uint16_t msg, uint32_t param) {
    // INPUT
    if (msg == WM_KEYDOWN) {
        uint32_t key = param;

        // SCROLLING LOGIC
        // Cardputer Arrow Keys usually send these codes via USB HID, 
        // but via 'status.word' on the device they might appear as special chars.
        // On Cardputer Lib, key ';'(up) and '.'(down) often map to arrows with Fn key.
        // For simplicity in this demo, we use '[' for UP and ']' for DOWN to test scrolling.
        
        if (key == ';') { // UP
            self->scrollY -= 10;
            if (self->scrollY < 0) self->scrollY = 0;
        }
        else if (key == '.') { // DOWN
            self->scrollY += 10;
            int maxScroll = self->contentHeight - (self->h - 20);
            if (self->scrollY > maxScroll) self->scrollY = maxScroll;
        }
        // OPEN POPUP KEY ('m')
        else if (key == 'm') {
             Window* popup = new Window("Alert", MessageBoxProc, TFT_LIGHTGREY);
             wm->openPopup(popup);
        }
        // TYPING
        else if (key == 8) { // Backspace
             if (self->stateText.length() > 0) self->stateText.remove(self->stateText.length() - 1);
        } 
        else if (key == 13) { // Enter
             self->stateText += '\n';
        }
        else {
             // Basic filter to avoid control chars
             if (key >= 32 && key <= 126) self->stateText += (char)key;
        }
    }

    // DRAWING
    if (msg == WM_PAINT) {
        self->drawStandardUI(globalCanvas);
        
        // Use a "Clip Rect" logic (Simple version: Don't draw if outside Y bounds)
        globalCanvas->setTextColor(TFT_WHITE);
        globalCanvas->setTextSize(1);
        
        int marginX = 5;
        int currentX = self->x + marginX;
        int startY = self->y + 20;
        int currentY = startY - self->scrollY; // Apply Scroll Offset
        int lineHeight = 10;
        int rightEdge = self->x + self->w - 10; // -10 for scrollbar space

        // 1. Calculate Content Height (Dry Run)
        // We need to know total height to draw scrollbar correctly
        // Real implementation would cache this, but for now we calc on fly
        int calcY = 0;
        int calcX = 0;
        for (int i = 0; i < self->stateText.length(); i++) {
             char c = self->stateText.charAt(i);
             int w = globalCanvas->textWidth(String(c));
             if (c == '\n' || calcX + w > (self->w - 15)) {
                 calcX = 0;
                 calcY += lineHeight;
             } else {
                 calcX += w;
             }
        }
        self->contentHeight = calcY + lineHeight;

        // 2. Actual Draw
        for (int i = 0; i < self->stateText.length(); i++) {
            char c = self->stateText.charAt(i);
            int charWidth = globalCanvas->textWidth(String(c));

            // Newline / Wrap
            if (c == '\n') {
                currentX = self->x + marginX;
                currentY += lineHeight;
                continue;
            }
            if (currentX + charWidth > rightEdge) {
                currentX = self->x + marginX;
                currentY += lineHeight;
            }

            // CLIPPING: Only draw if visible inside window
            // Top clip: currentY + lineHeight > self->y + 14 (Header)
            // Bottom clip: currentY < self->y + self->h
            if (currentY + lineHeight > self->y + 14 && currentY < self->y + self->h) {
                globalCanvas->setCursor(currentX, currentY);
                globalCanvas->print(c);
            }
            currentX += charWidth;
        }
    }
}

// --- APP: SYSTEM MONITOR ---
void SysMonProc(Window* self, uint16_t msg, uint32_t param) {
    if (msg == WM_PAINT) {
        self->drawStandardUI(globalCanvas);
        globalCanvas->setCursor(self->x + 5, self->y + 25);
        globalCanvas->setTextColor(TFT_GREEN);
        globalCanvas->print("System OK");
        
        globalCanvas->setCursor(self->x + 5, self->y + 40);
        globalCanvas->setTextColor(TFT_WHITE);
        globalCanvas->print("Press 'm' in");
        globalCanvas->setCursor(self->x + 5, self->y + 50);
        globalCanvas->print("Notepad for");
        globalCanvas->setCursor(self->x + 5, self->y + 60);
        globalCanvas->print("Popup Demo");
    }
}

// ==========================================
// 3. MAIN SETUP & LOOP
// ==========================================

M5Canvas display(&M5.Display);

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true); 
    
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    
    display.createSprite(240, 135);
    globalCanvas = &display;

    wm = new WindowManager(&display);

    wm->addWindow(new Window("Notepad", NotepadProc, TFT_NAVY));
    wm->addWindow(new Window("SysInfo", SysMonProc, 0x01E0)); 
}

void loop() {
    wm->update();
}