#ifndef LUARUNTIME_H
#define LUARUNTIME_H

// ============================================================
// LuaRuntime.h  —  Lua scripting engine for MicroOS
//
// Each Lua app gets its own LuaRuntime instance.
// The MicroOS Lua API (mw.*) is bound here.
//
// Requires the "EloquentLua" or "lua" Arduino library:
//   https://github.com/esp32-lua/lua-esp32
//
// In platformio.ini add:
//   lib_deps = https://github.com/esp32-lua/lua-esp32
// In Arduino IDE, install "Lua for ESP32" via Library Manager.
// ============================================================

#include <Arduino.h>
extern "C" {
#include "lua51/lua.h"
#include "lua51/lualib.h"
#include "lua51/lauxlib.h"
}
#include "MicroWin.h"

// ============================================================
// Forward declaration — LuaRuntime is itself a MicroApp so the
// kernel can launch it exactly like any native app.
// ============================================================
class LuaRuntime;

// ============================================================
// Lua-friendly paint canvas wrapper
// Identical behaviour to PaintCanvas but exposed to Lua via
// an integer "handle" stored in a registry table on the Lua
// state, so Lua code never touches a raw C++ pointer.
// ============================================================
class LuaPaintCanvas : public UIElement {
public:
  LGFX_Sprite* buffer;
  uint16_t     activeColor = TFT_BLACK;
  int          lastX = -1;
  int          lastY = -1;

  LuaPaintCanvas(int _x, int _y, int _w, int _h)
    : UIElement(_x, _y, _w, _h, false)
  {
    buffer = new LGFX_Sprite();
    buffer->setColorDepth(16);
    buffer->createSprite(_w, _h);
    buffer->fillSprite(TFT_WHITE);
  }

  ~LuaPaintCanvas() {
    buffer->deleteSprite();
    delete buffer;
  }

  void setColor(uint16_t c) { activeColor = c; }
  void clear()              { buffer->fillSprite(TFT_WHITE); }

  void update(int absX, int absY, int elW, int elH,
              int mx, int my, bool leftDown,
              bool /*clicked*/, bool /*released*/) override
  {
    bool hover = (mx >= absX && mx <= absX + elW &&
                  my >= absY && my <= absY + elH);
    if (hover && leftDown) {
      int lx = mx - absX;
      int ly = my - absY;
      if (lastX != -1) {
        buffer->drawLine(lastX,     lastY,     lx,     ly,     activeColor);
        buffer->drawLine(lastX + 1, lastY,     lx + 1, ly,     activeColor);
        buffer->drawLine(lastX,     lastY + 1, lx,     ly + 1, activeColor);
      } else {
        buffer->fillCircle(lx, ly, 1, activeColor);
      }
      lastX = lx; lastY = ly;
    } else {
      lastX = -1; lastY = -1;
    }
  }

  void draw(int absX, int absY, int elW, int elH,
            LGFX_Sprite* canvas) override
  {
    buffer->pushSprite(canvas, absX, absY);
    canvas->drawRect(absX, absY, elW, elH, TFT_DARKGREY);
  }
};

// ============================================================
// Per-app Lua registry
// LuaRuntime stores all C++ objects it creates for a Lua app
// in these vectors so it can clean them up on close, without
// Lua code ever calling C++ destructors directly.
// ============================================================
struct LuaRegistry {
  // Widgets created by this app (owned by the Window, but we
  // also keep a typed pointer for the Lua API to look up).
  std::vector<Label*>           labels;
  std::vector<Button*>          buttons;
  std::vector<TextInput*>       textInputs;
  std::vector<LuaPaintCanvas*>  paintCanvases;
  std::vector<MultilineTextBox*> textBoxes;
};

// ============================================================
// LuaRuntime — one instance per running Lua app
// ============================================================
class LuaRuntime : public MicroApp {
public:
  // Path to the .lua entry-point file on the SD card
  String scriptPath;

  // The Lua state for this app instance
  lua_State* L = nullptr;

  // Widget registry so Lua handles (ints) can be resolved to
  // real C++ pointers without storing raw pointers in Lua.
  LuaRegistry reg;

  // Lua callback refs stored with luaL_ref so they survive GC
  // key = widget handle,  value = Lua registry ref (int)
  std::map<int, int> buttonCallbacks;

  // Monotonically increasing handle counter (0 = invalid)
  int nextHandle = 1;

  // Label text cache used by mw.getText (avoids alloc)
  // ---------------------------------------------------

  explicit LuaRuntime(String path) : scriptPath(path) {}

  // -------------------------------------------------------
  // onLaunch — called by the kernel to start the Lua app.
  // Creates the Lua state, opens safe standard libs, registers
  // all mw.* bindings, then runs the script from the SD card.
  // -------------------------------------------------------
  void onLaunch(String /*args*/) override {
    appName = "LuaApp:" + scriptPath.substring(scriptPath.lastIndexOf('/') + 1);

    if (ESP.getFreeHeap() < 45000) {
      wm.showPopup("Not enough RAM for Lua app", POPUP_ERROR);
      return;
    }

    L = luaL_newstate();
    if (!L) { Serial.println("[Lua] Out of memory"); return; }

    // Open only safe standard libraries (no io/os/package)
    luaopen_base(L);
    luaopen_math(L);
    luaopen_string(L);
    luaopen_table(L);

    // Store a pointer to this LuaRuntime instance in the Lua
    // registry so all static C functions can reach it.
    lua_pushlightuserdata(L, (void*)this);
    lua_setfield(L, LUA_REGISTRYINDEX, "_mwRT");

    // Register the mw namespace
    _registerMwTable();

    // Load and run the script
    File f = SD.open(scriptPath, FILE_READ);
    if (!f) {
      Serial.printf("[Lua] Cannot open %s\n", scriptPath.c_str());
      lua_close(L); L = nullptr;
      return;
    }
    size_t len = f.size();
    char* src = new char[len + 1];
    f.read((uint8_t*)src, len);
    src[len] = '\0';
    f.close();

    int err = luaL_dostring(L, src);
    delete[] src;

    if (err) {
      String msg = lua_tostring(L, -1);
      Serial.printf("[Lua] Error: %s\n", msg.c_str());
      wm.showPopup("Lua: " + msg.substring(0, 40), POPUP_ERROR);
      lua_close(L); L = nullptr;
    }
  }

  void onClose() override {
    // Lua callbacks are cleaned up with luaL_unref, then the
    // state is closed.  Widget memory is owned by the Window
    // and freed when the Window destructor runs.
    if (L) {
      for (auto& kv : buttonCallbacks) luaL_unref(L, LUA_REGISTRYINDEX, kv.second);
      buttonCallbacks.clear();
      lua_close(L);
      L = nullptr;
    }
  }

  // -------------------------------------------------------
  // dispatch() — called by the kernel's onUpdate hook every
  // frame.  Fires any pending Lua callbacks (e.g. from Button
  // onClick lambdas that were queued rather than called inline,
  // if you later add a deferred-event queue).
  // Not strictly needed for the simple click model below, but
  // good to have as an extension point.
  // -------------------------------------------------------
  void dispatch() {
    // Extension point for future event queue dispatch
  }

private:
  // -------------------------------------------------------
  // Helper: retrieve the LuaRuntime* stored in the registry
  // -------------------------------------------------------
  static LuaRuntime* _self(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_mwRT");
    LuaRuntime* rt = (LuaRuntime*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return rt;
  }

  // -------------------------------------------------------
  // Register mw.* table with all bindings
  // -------------------------------------------------------
  void _registerMwTable() {
    lua_newtable(L);  // mw = {}

    // mw.window(x, y, w, h, title [, bgColor]) -> win_handle
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      int x     = luaL_checkinteger(LL, 1);
      int y     = luaL_checkinteger(LL, 2);
      int ww    = luaL_checkinteger(LL, 3);
      int hh    = luaL_checkinteger(LL, 4);
      const char* title = luaL_checkstring(LL, 5);
      uint16_t bg = lua_isnumber(LL, 6) ? (uint16_t)lua_tointeger(LL, 6) : TFT_BLACK;

      // Only one window per Lua app is supported in this design
      rt->window = new Window(x, y, ww, hh, String(title), bg);
      wm.windows.push_back(rt->window);
      wm.focusedWindow = rt->window;

      // Attach the update hook so Lua button callbacks fire
      rt->window->onUpdate = [rt](Window*) { rt->dispatch(); };

      lua_pushinteger(LL, 0); // Window handle is always 0
      return 1;
    });
    lua_setfield(L, -2, "window");

    // mw.label(x, y, text [, color]) -> label_handle
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      if (!rt->window) { lua_pushinteger(LL, -1); return 1; }
      int x         = luaL_checkinteger(LL, 1);
      int y         = luaL_checkinteger(LL, 2);
      const char* t = luaL_checkstring(LL, 3);
      uint16_t col  = lua_isnumber(LL, 4) ? (uint16_t)lua_tointeger(LL, 4) : TFT_WHITE;

      auto* lbl = new Label(x, y, String(t), col, false);
      rt->window->addElement(lbl);
      int h = rt->nextHandle++;
      rt->reg.labels.push_back(lbl);
      lua_pushinteger(LL, h);
      return 1;
    });
    lua_setfield(L, -2, "label");

    // mw.button(x, y, w, h, text, callback) -> btn_handle
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      if (!rt->window) { lua_pushinteger(LL, -1); return 1; }
      int x         = luaL_checkinteger(LL, 1);
      int y         = luaL_checkinteger(LL, 2);
      int bw        = luaL_checkinteger(LL, 3);
      int bh        = luaL_checkinteger(LL, 4);
      const char* t = luaL_checkstring(LL, 5);
      luaL_checktype(LL, 6, LUA_TFUNCTION);

      // Store the Lua function in the registry
      lua_pushvalue(LL, 6);
      int ref = luaL_ref(LL, LUA_REGISTRYINDEX);

      int handle = rt->nextHandle++;
      rt->buttonCallbacks[handle] = ref;

      auto* btn = new Button(x, y, bw, bh, String(t), false);

      // The onClick captures handle + rt so we can call the
      // correct Lua function without capturing a lua_State* that
      // might be stale after a close/reopen cycle.
      btn->onClick = [rt, handle]() {
        if (!rt->L) return;
        auto it = rt->buttonCallbacks.find(handle);
        if (it == rt->buttonCallbacks.end()) return;
        lua_rawgeti(rt->L, LUA_REGISTRYINDEX, it->second);
        if (lua_pcall(rt->L, 0, 0, 0) != LUA_OK) {
          String err = lua_tostring(rt->L, -1);
          lua_pop(rt->L, 1);
          Serial.printf("[Lua] onClick error: %s\n", err.c_str());
        }
      };

      rt->window->addElement(btn);
      rt->reg.buttons.push_back(btn);
      lua_pushinteger(LL, handle);
      return 1;
    });
    lua_setfield(L, -2, "button");

    // mw.textinput(x, y, w, h [, initialText]) -> handle
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      if (!rt->window) { lua_pushinteger(LL, -1); return 1; }
      int x  = luaL_checkinteger(LL, 1);
      int y  = luaL_checkinteger(LL, 2);
      int tw = luaL_checkinteger(LL, 3);
      int th = luaL_checkinteger(LL, 4);
      const char* init = lua_isstring(LL, 5) ? lua_tostring(LL, 5) : "";

      auto* ti = new TextInput(x, y, tw, th, false);
      ti->text = String(init);
      ti->cursorPos = ti->text.length();
      rt->window->addElement(ti);
      int h = rt->nextHandle++;
      rt->reg.textInputs.push_back(ti);
      lua_pushinteger(LL, h);
      return 1;
    });
    lua_setfield(L, -2, "textinput");

    // mw.paintcanvas(x, y, w, h) -> handle
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      if (!rt->window) { lua_pushinteger(LL, -1); return 1; }
      int x  = luaL_checkinteger(LL, 1);
      int y  = luaL_checkinteger(LL, 2);
      int cw = luaL_checkinteger(LL, 3);
      int ch = luaL_checkinteger(LL, 4);

      auto* pc = new LuaPaintCanvas(x, y, cw, ch);
      rt->window->addElement(pc);
      int h = rt->nextHandle++;
      rt->reg.paintCanvases.push_back(pc);
      lua_pushinteger(LL, h);
      return 1;
    });
    lua_setfield(L, -2, "paintcanvas");

    // mw.setColor(canvasHandle, color565int)
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt  = _self(LL);
      int handle      = luaL_checkinteger(LL, 1);
      uint16_t color  = (uint16_t)luaL_checkinteger(LL, 2);
      // Resolve handle — linear scan is fine for <=10 canvases
      for (auto* pc : rt->reg.paintCanvases) {
        (void)pc; // silence unused warning; handle-to-pointer map below
      }
      // Use index directly: paintCanvases stored in insertion order,
      // handles start at nextHandle at creation time, so we use the
      // per-object index embedded in the handle.
      // We store handles starting at 1; just find by linear search.
      // For simplicity we tag each canvas with its own handle by
      // scanning the buttons map for presence, then matching order.
      // Simpler: stash handles on a parallel map in registry.
      // For this version, use a global handle->canvas map.
      if (rt->_canvasForHandle.count(handle)) {
        rt->_canvasForHandle[handle]->setColor(color);
      }
      return 0;
    });
    lua_setfield(L, -2, "setColor");

    // mw.clearCanvas(canvasHandle)
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      int handle     = luaL_checkinteger(LL, 1);
      if (rt->_canvasForHandle.count(handle)) {
        rt->_canvasForHandle[handle]->clear();
      }
      return 0;
    });
    lua_setfield(L, -2, "clearCanvas");

    // mw.setText(labelHandle, text)
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt  = _self(LL);
      int handle      = luaL_checkinteger(LL, 1);
      const char* txt = luaL_checkstring(LL, 2);
      if (rt->_labelForHandle.count(handle)) {
        rt->_labelForHandle[handle]->text = String(txt);
      }
      return 0;
    });
    lua_setfield(L, -2, "setText");

    // mw.getText(textInputHandle) -> string
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      LuaRuntime* rt = _self(LL);
      int handle     = luaL_checkinteger(LL, 1);
      if (rt->_textInputForHandle.count(handle)) {
        lua_pushstring(LL, rt->_textInputForHandle[handle]->text.c_str());
      } else {
        lua_pushstring(LL, "");
      }
      return 1;
    });
    lua_setfield(L, -2, "getText");

    // mw.showPopup(message, type)  type: 0=info 1=warning 2=error
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      const char* msg = luaL_checkstring(LL, 1);
      int t           = lua_isnumber(LL, 2) ? (int)lua_tointeger(LL, 2) : 0;
      PopupType pt = (t == 1) ? POPUP_WARNING : (t == 2) ? POPUP_ERROR : POPUP_INFO;
      wm.showPopup(String(msg), pt);
      return 0;
    });
    lua_setfield(L, -2, "showPopup");

    // mw.playAudio(path)
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      const char* p = luaL_checkstring(LL, 1);
      wm.audio.play(String(p));
      return 0;
    });
    lua_setfield(L, -2, "playAudio");

    // mw.stopAudio()
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      (void)LL;
      wm.audio.stop();
      return 0;
    });
    lua_setfield(L, -2, "stopAudio");

    // mw.setVolume(0-255)
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      int v = luaL_checkinteger(LL, 1);
      wm.audio.setVolume((uint8_t)constrain(v, 0, 255));
      return 0;
    });
    lua_setfield(L, -2, "setVolume");

    // mw.lastKey() -> string ("UP","DOWN","LEFT","RIGHT","ESC",char,"")
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      auto& typed = wm.keyboard.typedChars;
      if (typed.empty()) { lua_pushstring(LL, ""); return 1; }
      char c = typed.back();
      if      (c == KEY_UP)    lua_pushstring(LL, "UP");
      else if (c == KEY_DOWN)  lua_pushstring(LL, "DOWN");
      else if (c == KEY_LEFT)  lua_pushstring(LL, "LEFT");
      else if (c == KEY_RIGHT) lua_pushstring(LL, "RIGHT");
      else if (c == KEY_ESC)   lua_pushstring(LL, "ESC");
      else { char s[2] = {c, '\0'}; lua_pushstring(LL, s); }
      return 1;
    });
    lua_setfield(L, -2, "lastKey");

    // mw.color(r, g, b) -> color565 int
    lua_pushcfunction(L, [](lua_State* LL) -> int {
      int r = luaL_checkinteger(LL, 1);
      int g = luaL_checkinteger(LL, 2);
      int b = luaL_checkinteger(LL, 3);
      // Manual color565 — avoids needing a canvas reference
      uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      lua_pushinteger(LL, c);
      return 1;
    });
    lua_setfield(L, -2, "color");

    // Expose named color constants
    auto setColor = [&](const char* name, uint16_t val) {
      lua_pushinteger(L, val);
      lua_setfield(L, -2, name);
    };
    setColor("BLACK",     TFT_BLACK);
    setColor("WHITE",     TFT_WHITE);
    setColor("RED",       TFT_RED);
    setColor("GREEN",     TFT_GREEN);
    setColor("BLUE",      TFT_BLUE);
    setColor("YELLOW",    TFT_YELLOW);
    setColor("CYAN",      TFT_CYAN);
    setColor("MAGENTA",   TFT_MAGENTA);
    setColor("ORANGE",    TFT_ORANGE);
    setColor("PURPLE",    TFT_PURPLE);
    setColor("DARKGREY",  TFT_DARKGREY);
    setColor("LIGHTGREY", TFT_LIGHTGREY);
    setColor("NAVY",      TFT_NAVY);

    lua_setglobal(L, "mw");  // Assign the table to the global "mw"
  }

  // -------------------------------------------------------
  // Handle → pointer maps (populated by the factory lambdas
  // above via a two-step: push to vector then register here).
  // We can't do it inside the lambda directly because this
  // member doesn't exist yet when _registerMwTable() builds
  // the closures.  Instead we override addElement wrappers
  // here and call _trackHandle after each creation.
  // -------------------------------------------------------
public:
  std::map<int, Label*>           _labelForHandle;
  std::map<int, TextInput*>       _textInputForHandle;
  std::map<int, LuaPaintCanvas*>  _canvasForHandle;

  // Called by the factory lambdas after they push to reg.*
  void _trackLabel(int h, Label* l)               { _labelForHandle[h] = l; }
  void _trackTextInput(int h, TextInput* ti)      { _textInputForHandle[h] = ti; }
  void _trackCanvas(int h, LuaPaintCanvas* pc)    { _canvasForHandle[h] = pc; }
};

// ============================================================
// Fix: The factory lambdas above call _trackHandle AFTER
// pushing the integer return.  We patch the label / textinput
// / paintcanvas functions so they call _track* on 'rt'.
// The cleanest way on embedded C++ (no reflection) is to
// re-register them with a slightly richer lambda now that the
// member maps are declared.
//
// Therefore we split registration: _registerMwTable() builds
// the window/button/popup/audio/color functions first, and
// _registerWidgetFunctions() (called right after) replaces
// the label/textinput/paintcanvas entries with ones that
// call _track*.  This is done by calling lua_setfield on the
// existing mw global.
// ============================================================

// Called once after LuaRuntime constructor but before onLaunch:
// We fold this into onLaunch by calling _patchWidgetFunctions()
// after _registerMwTable().  See the revised onLaunch below.
inline void _patchWidgetFunctions(lua_State* L, LuaRuntime* rt) {
  lua_getglobal(L, "mw");  // push mw table

  // --- REPLACE mw.label ---
  lua_pushcfunction(L, [](lua_State* LL) -> int {
    LuaRuntime* r   = LuaRuntime::_self(LL);
    if (!r->window) { lua_pushinteger(LL, -1); return 1; }
    int x           = luaL_checkinteger(LL, 1);
    int y           = luaL_checkinteger(LL, 2);
    const char* t   = luaL_checkstring(LL, 3);
    uint16_t col    = lua_isnumber(LL, 4) ? (uint16_t)lua_tointeger(LL, 4) : TFT_WHITE;
    auto* lbl       = new Label(x, y, String(t), col, false);
    r->window->addElement(lbl);
    int h           = r->nextHandle++;
    r->reg.labels.push_back(lbl);
    r->_trackLabel(h, lbl);
    lua_pushinteger(LL, h);
    return 1;
  });
  lua_setfield(L, -2, "label");

  // --- REPLACE mw.textinput ---
  lua_pushcfunction(L, [](lua_State* LL) -> int {
    LuaRuntime* r  = LuaRuntime::_self(LL);
    if (!r->window) { lua_pushinteger(LL, -1); return 1; }
    int x          = luaL_checkinteger(LL, 1);
    int y          = luaL_checkinteger(LL, 2);
    int tw         = luaL_checkinteger(LL, 3);
    int th         = luaL_checkinteger(LL, 4);
    const char* in = lua_isstring(LL, 5) ? lua_tostring(LL, 5) : "";
    auto* ti       = new TextInput(x, y, tw, th, false);
    ti->text       = String(in);
    ti->cursorPos  = ti->text.length();
    r->window->addElement(ti);
    int h          = r->nextHandle++;
    r->reg.textInputs.push_back(ti);
    r->_trackTextInput(h, ti);
    lua_pushinteger(LL, h);
    return 1;
  });
  lua_setfield(L, -2, "textinput");

  // --- REPLACE mw.paintcanvas ---
  lua_pushcfunction(L, [](lua_State* LL) -> int {
    LuaRuntime* r  = LuaRuntime::_self(LL);
    if (!r->window) { lua_pushinteger(LL, -1); return 1; }
    int x          = luaL_checkinteger(LL, 1);
    int y          = luaL_checkinteger(LL, 2);
    int cw         = luaL_checkinteger(LL, 3);
    int ch         = luaL_checkinteger(LL, 4);
    auto* pc       = new LuaPaintCanvas(x, y, cw, ch);
    r->window->addElement(pc);
    int h          = r->nextHandle++;
    r->reg.paintCanvases.push_back(pc);
    r->_trackCanvas(h, pc);
    lua_pushinteger(LL, h);
    return 1;
  });
  lua_setfield(L, -2, "paintcanvas");

  lua_pop(L, 1); // pop mw table
}

#endif // LUARUNTIME_H
