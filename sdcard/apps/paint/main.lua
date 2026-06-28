-- ============================================================
-- Paint App  (MicroOS Lua App)
-- File: /apps/paint/main.lua
--
-- A fully functional paint app using the mw.* API.
-- Color palette buttons + a clear button + a drawing canvas.
-- ============================================================

-- 1. Create the window
mw.window(10, 10, 220, 115, "Mini Paint", mw.color(230, 230, 230))

-- 2. Create the drawing canvas (below the toolbar, Y=23)
local canvas = mw.paintcanvas(5, 23, 210, 70)

-- 3. Color palette buttons (toolbar row)
mw.button(5,  2, 20, 18, "R", function()
  mw.setColor(canvas, mw.RED)
end)

mw.button(28, 2, 20, 18, "G", function()
  mw.setColor(canvas, mw.GREEN)
end)

mw.button(51, 2, 20, 18, "B", function()
  mw.setColor(canvas, mw.BLUE)
end)

mw.button(74, 2, 20, 18, "K", function()
  mw.setColor(canvas, mw.BLACK)
end)

mw.button(97, 2, 20, 18, "W", function()
  mw.setColor(canvas, mw.WHITE)
end)

mw.button(120, 2, 20, 18, "Y", function()
  mw.setColor(canvas, mw.YELLOW)
end)

mw.button(143, 2, 20, 18, "C", function()
  mw.setColor(canvas, mw.CYAN)
end)

-- 4. Clear button (far right)
mw.button(165, 2, 50, 18, "Clear", function()
  mw.clearCanvas(canvas)
end)
