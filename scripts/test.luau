local gui = require("@Luwow/gui")
local udc = require("@Luwow/udc")
local Color = require("@Luwow/color")
local UDim2 = require("@Luwow/udim2")
local task = require("@Luwow/task")

local window = gui.createWindow({
  Type = "Window" :: "Window",
  Title = "Luwow Created Window",
  Width = 400,
  Height = 300,
})

-- Native: the gui library's menu bar and button
local _menuBar = gui.createMenuBar({
  Type = "MenuBar" :: "MenuBar",
  Menus = {
    {
      Type = "Menu" :: "Menu",
      Title = "&File",
      Items = {
        {
          Type = "MenuItem" :: "MenuItem",
          Title = "&Demo",
          OnSelected = function()
            print("Hello from the menu")
          end,
        },
        {
          Type = "MenuItem" :: "MenuItem",
          Title = "&Quit",
          OnSelected = function()
            print("Quit selected")
            window:close()
          end,
        },
      },
    },
  },
}, window)

local _nativeButton = gui.createButton({
  Type = "Button" :: "Button",
  Caption = "Native",
  Left = 40,
  Top = 90,
  Width = 100,
  Height = 30,
  OnPressed = function()
    print("Native button pressed")
  end,
}, window)

-- Drawn: UDC's controls in the same window, given it as their Parent
local numPresses = 0
local _button = udc.createTextButton({
  Parent = window,
  Text = "Click me",
  OnActivated = function()
    numPresses += 1
    local epicNumpress = numPresses
    print("Button pressed " .. numPresses .. " times")
    task.delay(1, function()
        print(epicNumpress)
    end)
  end,
  Position = UDim2.fromOffset(40, 40),
  Size = UDim2.fromOffset(100, 30),
  BackgroundColor = Color.fromRGB(0, 120, 215),
  TextColor = Color.new(1, 1, 1),
})

-- udc.attach gives the UDC window drawing into it, the drawn controls' Parent
print(udc.attach(window) == _button.Parent)

window:show()

-- And a window of UDC's own
local own = udc.createWindow({
  Title = "UDC Window",
  Size = vector.create(300, 200),
})
udc.createTextLabel({ Parent = own, Text = "Drawn in UDC's own window", Size = UDim2.fromOffset(240, 30) })
own:Show()
