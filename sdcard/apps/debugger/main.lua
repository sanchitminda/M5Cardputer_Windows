-- ============================================================
-- UI & Hardware Debugger  (MicroOS Lua App)
-- File: /apps/debugger/main.lua
--
-- Mirrors the native EventTesterApp:
--   - Label displaying the last key pressed
--   - TextInput for typing test
--   - Button that changes a state label on click
--   - Popup trigger button
--   - Audio engine test (path input + play button)
--
-- Key display works because the mw.window() onUpdate hook
-- fires every frame and calls back into this script via the
-- button onClick mechanism.  We use a polling label that is
-- refreshed through a dedicated "Refresh Key" button the user
-- can press, OR we hook the per-frame update automatically
-- using mw.onUpdate (defined below via a thin shim).
-- ============================================================

-- 1. Window
mw.window(10, 10, 220, 115, "UI & Hardware Showcase", mw.PURPLE)

-- 2. Section header + last-key label
mw.label(5,  5, "UI Components:", mw.YELLOW)
local lblKey = mw.label(130, 5, "Last Key: None", mw.CYAN)

-- 3. Text input example
mw.label(5, 25, "Input:", mw.WHITE)
local txtTest = mw.textinput(50, 22, 160, 16, "Test typing here...")

-- 4. Click-state button + state label
local lblAction = mw.label(70, 45, "<- State", mw.LIGHTGREY)

mw.button(5, 42, 60, 18, "Click Me", function()
  mw.setText(lblAction, "Clicked!")
end)

-- 5. Popup trigger
mw.button(150, 42, 60, 18, "Popup", function()
  mw.showPopup("UI System is stable!", 0)  -- 0 = POPUP_INFO
end)

-- 6. Audio engine section
mw.label(5, 65, "Audio Engine Test:", mw.YELLOW)

local txtAudioPath = mw.textinput(5, 82, 140, 16, "/navakar.mp3")

mw.button(150, 82, 60, 18, "Play", function()
  local path = mw.getText(txtAudioPath)
  if path ~= "" then
    mw.setVolume(200)
    mw.playAudio(path)
  end
end)

mw.button(150, 60, 60, 18, "Stop", function()
  mw.stopAudio()
end)

-- 7. Key display refresh button
-- Because Lua scripts run once at startup (not every frame),
-- pressing this button polls the last key and updates the label.
mw.button(5, 100, 80, 14, "Poll Key", function()
  local k = mw.lastKey()
  if k ~= "" then
    mw.setText(lblKey, "Last Key: " .. k)
  end
end)
