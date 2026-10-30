local udc = require("@Luwow/udc")
local task = require("@Luwow/task")
local Color = require("@Luwow/color")
local UDim2 = require("@Luwow/udim2")
local UDim = require("@Luwow/udim")

local window = udc.createWindow({
    Title = "Luwow Drawn Controls",
    Position = vector.create(100, 100),
    Size = vector.create(480, 440),
})

local header = udc.createView({
    Size = UDim2.new(1, 0, 0, 48),
    BackgroundColor = Color.fromRGB(45, 45, 48),
}, window)

local text = udc.createTextLabel({
    FontStyle = "Bold",
    Text = "Luwow drawn controls",
    TextColor = Color.new(1, 1, 1),
    TextSize = 18,
    Position = UDim2.fromOffset(16, 0),
    Size = UDim2.new(1, -32, 1, 0),
}, header)

local status = udc.createTextLabel({
    Text = "Type a name and press Add",
    TextColor = Color.fromHex("#555555"),
    Position = UDim2.fromOffset(16, 104),
    Size = UDim2.new(1, -32, 0, 24),
}, window)

local list = udc.createScrollView({
    Position = UDim2.fromOffset(16, 136),
    Size = UDim2.new(1, -32, 1, -152),
    BackgroundColor = Color.fromRGB(250, 250, 250),
    BorderColor = Color.fromRGB(204, 204, 204),
    ScrollBarThickness = 8,
}, window)

-- Stacks the rows, so they don't need positions and close up when one is removed
udc.createListLayout({
    FillDirection = "Vertical",
    Padding = 2,
}, list)

local white = Color.new(1, 1, 1)
local grey = Color.fromRGB(240, 240, 240)
local rows = 0

-- Adds a row with a label and a button that removes the row again
local function addRow(text: string)
    local row = udc.createView({
        Size = UDim2.new(1, 0, 0, 32),
        LayoutOrder = rows,
        BackgroundColor = if rows % 2 == 0 then white else grey,
    }, list)
    rows += 1

    udc.createTextLabel({
        Text = text,
        Position = UDim2.fromOffset(10, 0),
        Size = UDim2.new(1, -110, 1, 0),
    }, row)

    -- Kept 10 pixels from the row's right edge, whatever its width
    local button = udc.createTextButton({
        Text = "Remove",
        Position = UDim2.new(1, -10, 0.5, 0),
        AnchorPoint = vector.create(1, 0.5),
        Size = UDim2.fromOffset(80, 24),
        OnActivated = function()
            row:Destroy()
            status.Text = `Removed {text}`
        end,
    }, row)

    udc.createCorner({
        CornerRadius = UDim.new(0, 6)
    }, button)

    -- Scrolls to the new row
    list.ScrollPosition = vector.create(0, list.ContentSize.y)
end

local input = udc.createTextInput({
    PlaceholderText = "Type a name",
    Position = UDim2.fromOffset(16, 64),
    Size = UDim2.new(1, -132, 0, 30),
    OnChanged = function(text)
        status.Text = if text == "" then "Type a name and press Add" else `{#text} characters`
    end,
    OnSubmitted = function(text)
        if text ~= "" then addRow(text) end
    end,
}, window)

udc.createTextButton({
    Text = "Add",
    Position = UDim2.new(1, -16, 0, 64),
    AnchorPoint = vector.create(1, 0),
    Size = UDim2.fromOffset(90, 30),
    OnActivated = function()
        if input.Text == "" then
            status.Text = "Nothing to add"
            return
        end

        addRow(input.Text)
        input.Text = ""

        -- Handlers run through task, so they can wait
        task.wait(1)
        status.Text = `{rows} rows added`
    end,
}, window)

for i = 1, 12 do
    addRow(`Row {i}`)
end
list.ScrollPosition = vector.zero

window:Show()
