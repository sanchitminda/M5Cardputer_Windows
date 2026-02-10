
# MicroWin for M5Cardputer

**MicroWin** is a lightweight, tiling window manager and UI framework designed specifically for the **ESP32-S3** based **M5Stack Cardputer**. It mimics the architecture of early Windows (1.0/2.0) but is optimized for small screens (240x135) and limited input methods (keyboard-only navigation).

## 1. Architecture Overview

The library operates on a **Message-Driven Architecture**. Just like standard Windows programming, applications do not run in their own loops. Instead, they are passive "Window Procedures" that wait for the system to send them a message (like `WM_PAINT` or `WM_KEYDOWN`).

### Core Components
* **WindowManager (`WM_Core`):** The kernel. It manages the list of open windows, calculates their layout (tiling), and dispatches input events from the hardware to the active window.
* **Window (`Window`):** An object representing a screen region. It holds state (size, position, title) and a pointer to its logic function.
* **Message Loop:** The `update()` method in the Manager, called inside the Arduino `loop()`. It keeps the UI alive.

## 2. API Reference

### Class: `Window`
Represents a single application or dialog box.

**Constructor:**
```cpp
Window(String title, WNDPROC proc, uint16_t bgColor)

```

* `title`: Text displayed in the top bar.
* `proc`: The function name that handles this window's logic.
* `bgColor`: The 16-bit color (e.g., `TFT_NAVY`) for the window background.

**Key Properties:**

* `x, y, w, h` (int): Calculated automatically by the WindowManager. Read-only for apps.
* `stateText` (String): A generic string buffer for simple apps (like Notepad) to store content.
* `scrollY` (int): The vertical scroll offset in pixels.
* `contentHeight` (int): The total height of the content (used to calculate scrollbar size).

**Helper Methods:**

* `drawStandardUI(M5Canvas* canvas)`: Draws the standard Title Bar, Borders, and Background. Call this first in your `WM_PAINT` handler.

### Class: `WindowManager`

The engine that runs the system.

**Methods:**

* `addWindow(Window* w)`: Registers a new app and recalculates the screen split.
* `openPopup(Window* popup)`: Opens a window in "Modal" mode. It floats in the center, dims the background, and captures all keyboard input until closed.
* `closePopup()`: Destroys the active popup and returns focus to the main windows.
* `update()`: **Must be called in `void loop()**`. Handles input polling and screen refreshing.

## 3. The Window Procedure (`WNDPROC`)

Every app must be defined as a function with this exact signature:

```cpp
void AppNameProc(Window* self, uint16_t msg, uint32_t param);

```

### Message Types

| Message Constant | Description | `param` Usage |
| --- | --- | --- |
| `WM_CREATE` | Sent when window is first added. | Unused (0). |
| `WM_PAINT` | Sent every frame. **Draw your UI here.** | Unused (0). |
| `WM_KEYDOWN` | Sent when a key is pressed and this window has focus. | Contains the ASCII char or Key Code. |
| `WM_TIMER` | Sent periodically (optional implementation). | Current `millis()` time. |
| `WM_CLOSE` | Sent when the window is being destroyed. | Unused (0). |

## 4. How to Write an Application

Here is a boilerplate template for creating a new "App" in MicroWin.

### Step 1: Define the Logic

```cpp
void MyAppProc(Window* self, uint16_t msg, uint32_t param) {
    // 1. Handle Input
    if (msg == WM_KEYDOWN) {
        char key = (char)param;
        if (key == 'x') {
            // Do something when 'x' is pressed
        }
    }

    // 2. Handle Drawing
    if (msg == WM_PAINT) {
        // Draw the standard window frame
        self->drawStandardUI(globalCanvas);
        
        // Draw your custom content relative to self->x and self->y
        globalCanvas->setTextColor(TFT_WHITE);
        globalCanvas->setCursor(self->x + 5, self->y + 20);
        globalCanvas->print("My Custom App");
    }
}

```

### Step 2: Register the App

In your Arduino `setup()` function:

```cpp
// Create the window and add it to the manager
wm->addWindow(new Window("My App", MyAppProc, TFT_DARKGREEN));

```

## 5. User Guide (Keyboard Shortcuts)

Once MicroWin is running on the Cardputer, these are the global controls:

| Key | Action |
| --- | --- |
| **TAB** | Switch focus between open windows (Cycle). |
| **OPT** | **Maximize/Restore.** Toggles the active window between full-screen and split-screen. |
| **Fn + Left/Right** | **Resize Split.** (In split mode) Adjusts the width of the two windows. |
| **M** | **Demo Popup.** Opens the "System Alert" modal (if implemented in app). |
| **ENTER** | Confirm / New Line / Close Popup. |

## 6. Known Limitations

* **Memory:** Each `Window` object consumes heap memory. On ESP32-S3, you can safely open 5-10 simple windows, but heavy text buffers may cause crashes.
* **Flicker:** The system redraws the entire screen every loop. for complex graphics, this may cause flickering.
* *Fix:* Use `canvas->pushSprite()` only once at the very end of the `WindowManager::update()` loop (already implemented in v1.0).


* **Input:** The current input handler maps raw USB HID keys to ASCII. Some special keys (PageUp, Home, End) may need custom mapping in `WM_KEYDOWN`.

```

```
