#ifndef APPLOADER_H
#define APPLOADER_H

// ============================================================
// AppLoader.h  —  SD-card Lua app discovery & registration
//
// At boot, call AppLoader::scan(wm) and it will:
//   1. Enumerate every directory under /apps/ on the SD card
//   2. Read its app.json manifest
//   3. Register a LuaRuntime factory in the WindowManager
//   4. Push a DesktopIcon if an icon drawing function is given,
//      or fall back to a generic "Lua" icon
//
// Manifest format (app.json):
// {
//   "name":         "Paint",          <- display name & registry key
//   "entry":        "main.lua",       <- Lua file inside the app dir
//   "iconX":        80,               <- desktop icon X position
//   "iconY":        20,               <- desktop icon Y position
//   "associations": [".foo", ".bar"]  <- optional file extensions
// }
//
// Minimal manifest (only name + entry are required):
// { "name":"Counter", "entry":"main.lua" }
// ============================================================

#include <Arduino.h>
#include <SD.h>
#include "MicroWin.h"
#include "LuaRuntime.h"

// ============================================================
// A tiny JSON value extractor — avoids pulling in ArduinoJson
// just for four fields.  Handles only flat string values and
// flat string arrays; good enough for our manifest format.
// ============================================================
namespace ManifestParser {
  // Returns the value of "key":"value" from a JSON string.
  // Returns "" if not found.
  static String getString(const String& json, const char* key) {
    String needle = "\"";
    needle += key;
    needle += "\"";
    int ki = json.indexOf(needle);
    if (ki < 0) return "";
    int colon = json.indexOf(':', ki + needle.length());
    if (colon < 0) return "";
    int quote1 = json.indexOf('"', colon + 1);
    if (quote1 < 0) return "";
    int quote2 = json.indexOf('"', quote1 + 1);
    if (quote2 < 0) return "";
    return json.substring(quote1 + 1, quote2);
  }

  // Returns the integer value of "key":number.
  // Returns defaultVal if not found.
  static int getInt(const String& json, const char* key, int defaultVal = -1) {
    String needle = "\"";
    needle += key;
    needle += "\"";
    int ki = json.indexOf(needle);
    if (ki < 0) return defaultVal;
    int colon = json.indexOf(':', ki + needle.length());
    if (colon < 0) return defaultVal;
    // Skip whitespace
    int i = colon + 1;
    while (i < (int)json.length() && json[i] == ' ') i++;
    if (i >= (int)json.length()) return defaultVal;
    if (!isdigit(json[i]) && json[i] != '-') return defaultVal;
    return json.substring(i).toInt();
  }

  // Fills 'out' with string values from "key":["val1","val2",...]
  static void getStringArray(const String& json, const char* key,
                             std::vector<String>& out)
  {
    String needle = "\"";
    needle += key;
    needle += "\"";
    int ki = json.indexOf(needle);
    if (ki < 0) return;
    int bracket = json.indexOf('[', ki);
    if (bracket < 0) return;
    int end = json.indexOf(']', bracket);
    if (end < 0) return;
    String section = json.substring(bracket + 1, end);
    int pos = 0;
    while (true) {
      int q1 = section.indexOf('"', pos);
      if (q1 < 0) break;
      int q2 = section.indexOf('"', q1 + 1);
      if (q2 < 0) break;
      out.push_back(section.substring(q1 + 1, q2));
      pos = q2 + 1;
    }
  }
}

// ============================================================
// Generic fallback desktop icon for Lua apps that don't
// supply a custom draw function.  Draws a simple "L" badge.
// ============================================================
static void drawLuaIcon(LGFX_Sprite* canvas, int x, int y) {
  // Dark background pill
  canvas->fillRoundRect(x + 2, y + 2, 20, 20, 3, canvas->color565(30, 60, 90));
  // Bright "L" lettermark
  canvas->setTextColor(TFT_CYAN);
  canvas->setTextSize(2);
  canvas->setTextDatum(middle_center);
  canvas->drawString("L", x + 12, y + 12);
  canvas->setTextDatum(top_left);
  canvas->setTextSize(1);
}

// ============================================================
// AppLoader
// ============================================================
class AppLoader {
public:
  // -------------------------------------------------------
  // scan() — call once in setup() after SD.begin() and
  // wm.begin().  Discovers all valid Lua apps and registers
  // them with the kernel's app registry + icon list.
  // -------------------------------------------------------
  static void scan(WindowManager& wm,
                   const String& appsRoot = "/apps")
  {
    if (!SD.exists(appsRoot)) {
      Serial.printf("[AppLoader] %s not found on SD card\n",
                    appsRoot.c_str());
      return;
    }

    File root = SD.open(appsRoot);
    if (!root || !root.isDirectory()) {
      Serial.printf("[AppLoader] Cannot open %s\n", appsRoot.c_str());
      return;
    }

    File entry = root.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
        String dirPath = appsRoot + "/" + String(entry.name());
        _tryLoadApp(wm, dirPath);
      }
      entry.close();
      entry = root.openNextFile();
    }
    root.close();

    Serial.printf("[AppLoader] Scan complete.\n");
  }

private:
  // -------------------------------------------------------
  // Attempt to load one app directory.
  // Fails silently if app.json is missing or malformed.
  // -------------------------------------------------------
  static void _tryLoadApp(WindowManager& wm, const String& dirPath) {
    String manifestPath = dirPath + "/app.json";

    if (!SD.exists(manifestPath)) {
      Serial.printf("[AppLoader] No app.json in %s, skipping.\n",
                    dirPath.c_str());
      return;
    }

    // --- 1. Read the manifest ---
    File mf = SD.open(manifestPath, FILE_READ);
    if (!mf) return;

    String json = "";
    while (mf.available()) json += (char)mf.read();
    mf.close();

    // --- 2. Parse required fields ---
    String appName  = ManifestParser::getString(json, "name");
    String entryFile = ManifestParser::getString(json, "entry");

    if (appName.length() == 0 || entryFile.length() == 0) {
      Serial.printf("[AppLoader] %s: missing name or entry, skipping.\n",
                    dirPath.c_str());
      return;
    }

    String entryPath = dirPath + "/" + entryFile;
    if (!SD.exists(entryPath)) {
      Serial.printf("[AppLoader] %s: entry %s not found, skipping.\n",
                    dirPath.c_str(), entryPath.c_str());
      return;
    }

    // --- 3. Parse optional fields ---
    int iconX = ManifestParser::getInt(json, "iconX", -1);
    int iconY = ManifestParser::getInt(json, "iconY", -1);

    std::vector<String> associations;
    ManifestParser::getStringArray(json, "associations", associations);

    // --- 4. Register app factory ---
    // entryPath is captured by value so the lambda is self-contained.
    wm.registerApp(appName, [entryPath]() -> MicroApp* {
      return new LuaRuntime(entryPath);
    });

    Serial.printf("[AppLoader] Registered Lua app \"%s\" -> %s\n",
                  appName.c_str(), entryPath.c_str());

    // --- 5. Register file associations ---
    for (auto& ext : associations) {
      wm.associateExt(ext, appName);
      Serial.printf("[AppLoader]   assoc %s -> %s\n",
                    ext.c_str(), appName.c_str());
    }

    // --- 6. Push a desktop icon (if position given) ---
    if (iconX >= 0 && iconY >= 0) {
      // Capture appName by value for the lambda
      String capturedName = appName;
      wm.icons.push_back(
        new DesktopIcon(iconX, iconY, appName, drawLuaIcon, [&wm, capturedName]() {
          wm.launchApp(wm.appRegistry[capturedName](), "");
          return (Window*)nullptr;
        })
      );
      Serial.printf("[AppLoader]   icon at (%d, %d)\n", iconX, iconY);
    }
  }
};

#endif // APPLOADER_H
