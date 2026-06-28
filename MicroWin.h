// MicroWin.h — Umbrella header for the MicroOS UI framework.
// Include this single file in any sketch or library that needs the full OS.
//
// Include order is dependency-driven (see core/Window.h for details):
//   HAL → FreeRTOS bridge → MicroApp → UIElement → simple widgets →
//   Window declaration → DesktopIcon → WindowManager →
//   Window implementations → input widgets → PopupWindow
//
// The monolithic MicroWin.h has been split into focused sub-headers.
// To add a new widget: create ui/MyWidget.h extending UIElement, then
// add its #include below in the correct dependency tier.

#pragma once
#ifndef MICROWIN_H
#define MICROWIN_H

// ---- System / Arduino / LGFX ----
#include "lgfx/v1/misc/enum.hpp"
#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>
#include <deque>
#include <functional>
#include <algorithm>
#include <map>

// ---- USB HID (required by MouseDevice) ----
#include <class/hid/hid.h>
#include "EspUsbHost.h"

// ---- Audio libraries (required by hal/AudioDevice.h) ----
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutput.h>
#include <AudioFileSourceID3.h>

// ============================================================
// TIER 1 — Hardware Abstraction Layer
// No dependencies on UI classes
// ============================================================
#include "hal/MouseDevice.h"     // KEY_*, enums, MouseDevice
#include "hal/KeyboardDevice.h"  // KeyboardDevice
#include "hal/AudioDevice.h"     // AudioOutputM5Speaker, AudioDeviceSimple

// ============================================================
// TIER 2 — Core infrastructure
// ============================================================
#include "core/FreeRTOSBridge.h" // inline _appTaskWrapper
#include "core/MicroApp.h"       // MicroApp base class (fwd-decl Window*)
#include "core/UIElement.h"      // UIElement base class

// ============================================================
// TIER 3 — Simple widgets (no WindowManager dependency)
// ============================================================
#include "ui/Label.h"
#include "ui/Button.h"
#include "ui/VirtualListView.h"  // FileEntry + VirtualListView

// ============================================================
// TIER 4 — Window declaration
// Declares extern WindowManager wm (exactly once, here)
// ============================================================
#include "core/Window.h"

// ============================================================
// TIER 5 — Desktop icon (needs Window* return type)
// ============================================================
#include "ui/DesktopIcon.h"

// ============================================================
// TIER 6 — WindowManager full definition
// showPopup() declared here, defined in PopupWindow.h
// ============================================================
#include "core/WindowManager.h"

// ============================================================
// TIER 7 — Window method bodies
// Must come after WindowManager is fully defined
// ============================================================
#include "core/WindowImpl.h"

// ============================================================
// TIER 8 — Input widgets (need wm.keyboard)
// ============================================================
#include "ui/TextInput.h"
#include "ui/MultilineTextBox.h"

// ============================================================
// TIER 9 — Draw-only widgets (no wm dependency)
// ============================================================
#include "ui/PaintCanvas.h"

// ============================================================
// TIER 10 — PopupWindow + WindowManager::showPopup impl
// Must come last: needs Button, Label, Window, WindowManager
// ============================================================
#include "ui/PopupWindow.h"

#endif // MICROWIN_H
