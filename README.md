# MicroWin OS for M5Cardputer

**MicroWin** is a lightweight, object-oriented, overlapping Window Manager and GUI framework built specifically for the ESP32-S3 M5Stack Cardputer. It features a complete desktop metaphor, dynamic memory management, component-based UI elements, and hardware-agnostic input handling.

## Key Features

* **Desktop Metaphor:** Launch apps via desktop icons.
* **Overlapping Windows (Z-Ordering):** Windows can be dragged over each other. Clicking a background window brings it to the top.
* **Dynamic RAM Management:** Uses the Factory Pattern to build apps only when launched, and a Garbage Collector to `delete` them and free heap memory when closed.
* **Window Controls:** Fully functional Title Bar with Dragging, Minimizing (hide), Maximizing (fullscreen), and Closing (destroy).
* **Responsive Layouts:** Support for both Fixed (pixel-perfect) and Relative (percentage-based) positioning and sizing. Elements automatically scale when a window is maximized or resized.
* **Keyboard & Focus Routing:** The OS intelligently routes M5Cardputer physical keyboard strokes only to the currently focused window and its focused `TextInput` elements.
* **Hardware Agnostic:** The library does not care where your input comes from (USB Host Mouse, I2C Joystick, Touchscreen). You simply feed it `(X, Y, Click)` coordinates in your main loop.

---

## Architecture Overview

MicroWin is built on three main pillars:

1. **`WindowManager` (The OS Kernel):** Manages the Desktop, the array of active `Window` objects, Z-ordering, Garbage Collection, and event dispatching.
2. **`Window` (The App Container):** Holds the state of an application, handles title bar interactions, and acts as a container for `UIElement` objects.
3. **`UIElement` (The Components):** The base class for everything inside a window (`Button`, `Label`, `TextInput`, etc.). Elements only know their position relative to their parent window.

---

## Built-in UI Components

All components inherit from `UIElement(x, y, w, h, isRelative)`.

* If `isRelative` is `false`, coordinates are exact pixels.
* If `isRelative` is `true`, coordinates are percentages (0-100) of the parent window's size.

### 1. `Label`

Displays static or dynamic text.

```cpp
Label(int x, int y, String text, uint16_t color = TFT_WHITE, bool isRel = false)

```

### 2. `Button`

Clickable component with hover and press states. Uses `std::function` for event binding.

```cpp
Button(int x, int y, int w, int h, String text, bool isRel = false)

```

* **Events:** `btn->onClick = [](){ /* logic */ };`

### 3. `TextInput`

A focusable text box that captures physical keyboard inputs and displays a blinking cursor.

```cpp
TextInput(int x, int y, int w, int h, bool isRel = false)

```

### 4. `PaintCanvas`

A specialized element that records mouse drag movements into a highly memory-efficient `std::vector` of vector lines rather than using a heavy bitmap buffer.

```cpp
PaintCanvas(int x, int y, int w, int h, bool isRel = false)

```

---

## How to Create a New Application

MicroWin uses the **Factory Pattern** to save RAM. Instead of creating windows globally, you create a lambda function (the "blueprint") that returns a pointer to a newly built `Window`. The `DesktopIcon` executes this blueprint only when the user clicks the icon.

### Step 1: Write the App Factory

Define the window, add elements to it, and bind your logic using lambda functions.

```cpp
auto createMyApp = []() -> Window* {
  // 1. Create the Window (x, y, w, h, title, background_color)
  Window* win = new Window(20, 20, 160, 90, "My App", TFT_DARKCYAN);

  // 2. Add UI Elements
  Label* myLabel = new Label(10, 15, "Status: Ready", TFT_WHITE, false);
  win->addElement(myLabel);

  // 3. Create a Button and Bind an Event
  Button* myBtn = new Button(10, 40, 80, 25, "Click Me!", false);
  
  // Notice we capture [myLabel] so the button can modify it
  myBtn->onClick = [myLabel]() {
    myLabel->text = "Status: Clicked!";
  };
  win->addElement(myBtn);

  // 4. (Optional) Hook into the Window's per-frame update loop
  win->onUpdate = [](Window* self) {
    // Code here runs every frame while the window is open
    // Great for updating clocks, polling sensors, or system stats
  };

  return win; // Return the fully built window
};

```

### Step 2: Register the App on the Desktop

Pass your factory function into a new `DesktopIcon` and add it to the Window Manager.

```cpp
// DesktopIcon(x, y, label, icon_color, factory_function)
wm.icons.push_back(new DesktopIcon(10, 10, "My App", TFT_PURPLE, createMyApp));

```

---

## Hardware Integration (The `.ino` File)

Because `MicroWin` is hardware-agnostic, you must pass the hardware state into `wm.update()` every frame. Here is the minimal boilerplate:

```cpp
#include <Arduino.h>
#include <M5Cardputer.h>
#include "MicroWin.h" // Your custom library

// 1. Create the Display Canvas & OS Instance
LGFX_Sprite canvas(&M5Cardputer.Display);
WindowManager wm;

// 2. Define your Input Variables (Update these via USB Host, Joystick, etc.)
int mouse_x = 120;
int mouse_y = 67;
bool mouse_click = false;

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  canvas.createSprite(240, 135);
  
  // Register Apps...
  // wm.icons.push_back(new DesktopIcon(..., createMyApp));
}

void loop() {
  // Update your mouse_x, mouse_y, and mouse_click variables here
  
  // Feed the hardware state into the OS
  wm.update(mouse_x, mouse_y, mouse_click, &canvas);
}

```

---

## Global Keyboard Shortcuts

* **Tab:** Cycles focus to the next open window (brings the bottom window to the top).
* **Backspace:** Deletes characters in focused `TextInput` components.
* **Alphanumeric Keys:** Routed automatically to the focused window's `TextInput` element.
