# UDC
User-Drawn Controls: a cross-platform library of windows and controls Luwow-Project draws itself.

This repository is not stand-alone. You must build it externally from within another
repository.

## Project Overview

This project contains the sources for Luwow's UDC library, required as `@Luwow/udc`.

The included scripts in the scripts folder demonstrate the basic functionality of the library.

## Drawn Controls

The controls are painted by the library itself, so they look and behave the same on every platform.

| Function | Control |
|---|---|
| `udc.createView(descriptor, parent)` | A container with an optional background and border |
| `udc.createScrollView(descriptor, parent)` | A container scrolled with the mouse wheel or its scroll bars |
| `udc.createTextLabel(descriptor, parent)` | Text, on one line or several |
| `udc.createImageLabel(descriptor, parent)` | An image from a file |
| `udc.createTextButton(descriptor, parent)` | A button with text, `OnActivated` |
| `udc.createImageButton(descriptor, parent)` | A button with an image, `OnActivated` |
| `udc.createTextInput(descriptor, parent)` | A text box, `OnChanged`, `OnSubmitted`, `OnFocusLost` |
| `udc.createListLayout(descriptor, parent)` | Arranges its parent's other children in a row or column |
| `udc.createCorner(descriptor, parent)` | Rounds its parent's corners |
| `udc.createPadding(descriptor, parent)` | Insets its parent's content |
| `udc.createStroke(descriptor, parent)` | Outlines its parent |
| `udc.createGradient(descriptor, parent)` | Colors its parent's background along a line |

### Creating a simple window with a button and input

```luau
local udc = require("@Luwow/udc")
local Color = require("@Luwow/color")
local UDim2 = require("@Luwow/udim2")

local window = udc.createWindow({ Title = "Hello", Size = vector.create(400, 300) })

local input = udc.createTextInput({ Position = UDim2.fromOffset(16, 16), Size = UDim2.new(1, -152, 0, 30) }, window)
udc.createTextButton({
    Text = "Say hello",
    Position = UDim2.new(1, -16, 0, 16),
    AnchorPoint = vector.create(1, 0),
    Size = UDim2.fromOffset(110, 30),
    BackgroundColor = Color.fromRGB(0, 120, 215),
    TextColor = Color.new(1, 1, 1),
    OnActivated = function()
        print("Hello, " .. input.Text)
    end,
}, window)

window:Show()
```

### Creating a popup

Popups are windows too. One with an `Owner` stays above that window, and a `Modal` one turns the owner's input off while it's shown. `PreventClose` leaves the close button to `OnCloseRequested`, so a script can ask before closing, and `OnClosed` fires once a window is closed, by the user or by `Close`.

```luau
local dialog = udc.createWindow({
    Title = "Confirm",
    ClientSize = vector.create(320, 120),
    Resizable = false,
    Minimizable = false,
    Owner = window,
    Modal = true,
    Centered = true,
    OnClosed = function()
        print("dialog closed")
    end,
})
udc.beep("Warning")
dialog:Show()
```

### Code layout

Each drawn control is its own class in its own files under `src/Controls`. A class's files hold everything about it, including its Luau members, its `RegisterClass` and the descriptor fields it reads:

| Files | |
|---|---|
| `ControlObject.*` | The base class of every control: its place in the tree, placement, colors, painting, input and handlers, and the Luau members every control has |
| `TextObject.*` | The base of the text controls, and their shared Luau members |
| `ImageObject.*` | The base of the image controls, and their shared Luau members |
| `Corner.*` | Rounds its parent's corners |
| `Gradient.*` | Colors its parent's background, and works out the gradient's stops |
| `View.*`, `ScrollView.*`, `TextLabel.*`, `TextButton.*`, `TextInput.*`, `ImageLabel.*`, `ImageButton.*`, `ListLayout.*` | One per control |
| `ControlView.*` | A window's control tree: painting, input routing, layout passes, handlers |
| `ControlRegistry.*` | Registers every control class with Luau, and the `udc.create*` functions |
| `ControlLuau.*` | Helpers the classes' Luau members share: values, descriptor fields, member tables |
| `Canvas.h`, `Scaling.*` | What a platform implements to draw controls, and the high-DPI conversion |

Controls are drawn through the virtual `ICanvas` interface, and the platforms implement it. Under `src/`, you can find every platform's implementation, such as `GdiCanvas.*` on Windows and `CocoaCanvas.*` on macOS.

### Types

```
BaseObject                 ClassName, Name, Destroyed, IsA(), Destroy(), from the baseobject library
└─ BaseGuiObject           Parent, FetchChildren(), BindToPropertyChanged()
   ├─ Window               Title, Owner, Modal, Enabled, TopMost, PreventClose, ClientSize, Visible, Show(),
   │                       Hide(), Close(), Focus(), Center(), SetClipboardText(), OnCloseRequested, OnClosed,
   │                       OnFocused
   ├─ ListLayout
   ├─ Corner
   ├─ Padding              PaddingTop, PaddingBottom, PaddingLeft, PaddingRight
   ├─ Stroke               Color, Thickness, StrokePosition
   ├─ Gradient             Color, Transparency, Rotation, Offset, Enabled
   └─ ControlObject        Position, Size, AnchorPoint, MinSize, MaxSize, AspectRatio, Flex, AutomaticSize,
      │                    Visible, ClipsDescendants, LayoutOrder, colors, AbsolutePosition/Size/Frame, OnEnter, OnLeft,
      │                    OnPressed, OnUnPressed
      ├─ View
      ├─ ScrollView        ScrollPosition, ContentSize, AutomaticCanvasSize, ScrollBarColor, ScrollBarThickness
      ├─ ImageObject       Image, ScaleType, ImageSize, ImageRectOffset, ImageRectSize, ImageColor,
      │                    SliceCenter, SliceScale, TileSize, Rotation
      │  ├─ ImageLabel
      │  └─ ImageButton    HoverImage, PressedImage, HoverColor, PressedColor
      └─ TextObject        Text, TextColor, TextSize, Font, TextAlignment, MultiLine, TextWrapped, Truncated
         ├─ TextLabel
         ├─ TextButton     HoverColor, PressedColor
         └─ TextInput      PlaceholderText, Focused, Focus(), ReleaseFocus(), ...
```

`IsA` follows the same tree: a `TextButton` IsA `"TextObject"`, `"ControlObject"`, `"BaseGuiObject"` and `"BaseObject"`. 2D values such as `AbsoluteSize` are `Vector2`s: vectors with x and y used, and z left at 0.

In C++, the classes mirror the Luau types: `TextButton` inherits `TextObject`, which inherits `ControlObject`, which inherits `BaseGuiObject` (`src/Controls/BaseGuiObject.*`), which inherits the baseobject library's `BaseObject`. 

`ListLayout`, `Corner`, `Padding`, `Stroke` and `Gradient` also inherit `ControlObject`, as helpers that aren't drawn, but declare `BaseGuiObject` as their `Super`, so scripts don't see them as `ControlObject`s.

### Values

The value types are their own libraries, usable from Luau and C++ (see their READMEs):

| Type | Library | Used for |
|---|---|---|
| `Color` | `@Luwow/color` | Every color property, drawn blended by its alpha |
| `Rect` | `@Luwow/rect` | `AbsoluteFrame`, and the C++ `Rect` controls are laid out and painted with |
| `UDim`, `UDim2` | `@Luwow/udim`, `@Luwow/udim2` | `Position`, `Size`, `MinSize`, `MaxSize`, `ListLayout.Padding` |
| `vector` | Luau's built in `vector.create(x, y)` | Pixels: `AnchorPoint`, `Absolute*`, `ScrollPosition`, native positions and sizes |

UDC registers these types itself, so values read from controls work even when a script doesn't require their library. A color property set to `nil` (or `false` in a descriptor) draws nothing.

### Drawing order

`ZIndex` (1 by default) orders a control among its siblings: a higher one is drawn on top and gets the mouse first, equal ones keep the order they were created in.

### Sizing

Each axis is a fraction of the parent's content (`Scale`) plus pixels (`Offset`). The properties take a UDim2 or a vector of pixels.

- A control is the full size of its parent by default, `UDim2.fromScale(1, 1)`, and `0` means zero, not fill.
- `AnchorPoint` picks the point placed at the position: `vector.create(0.5, 0.5)` with a position of `UDim2.fromScale(0.5, 0.5)` centers a control.
- `MinSize` and `MaxSize` limit the resolved size, and the minimum wins when they conflict.
- Percentages refer to the parent's content box, inside its border.

### Images

An `ImageLabel` shows an image file (PNG, JPEG, BMP or GIF):

```luau
udc.createImageLabel({ Image = "Assets/Logo.png", ScaleType = "Fit", Size = UDim2.fromOffset(96, 96) }, window)
```

`ScaleType` is:

* `"Stretch"`: fills the control
* `"Fit"`: fills the whole image, proportions kept
* `"Crop"`: fills the control, proportions kept, the rest cut off
* `"Slice"`: nine-sliced, the borders around `SliceCenter` keep their size times `SliceScale`, shrinking together when they don't fit
* `"Tile"` repeated at `TileSize`, a UDim2 against the content. 

`ImageSize` is the image's size in pixels, `0, 0` when it couldn't be loaded. The image is drawn over the control's background and border, inside the border.
`ImageRectOffset` and `ImageRectSize` pick the part of the image shown, such as an icon from a sprite sheet (a size of `0, 0` shows it all), and `ImageColor` multiplies its colors, its alpha being the image's opacity. `Rotation` turns the image, not the control or its children, clockwise about its center, such as a loading spinner.

```luau
udc.createImageLabel({ Image = "assets/icons.png", ImageRectOffset = vector.create(64, 0), ImageRectSize = vector.create(32, 32) }, window)
udc.createImageLabel({ Image = "assets/panel.png", ScaleType = "Slice", SliceCenter = Rect.new(10, 10, 12, 12) }, window)
```

Images are loaded once per window and shared by every control showing the same path.

### Gradients

A `Gradient` placed in a control colors its background along a line, at each point the background's color is multiplied by the gradient's, and its opacity by the gradient's.

```luau
local banner = udc.createView({ BackgroundColor = Color.new(1, 1, 1), Size = UDim2.fromOffset(200, 60) }, window)
udc.createGradient({ Color = { Color.new(1, 0, 0), Color.new(0, 0, 1) }, Rotation = 90 }, banner)
udc.createGradient({ Color = { { Time = 0, Value = Color.new(1, 1, 0) }, { Time = 0.3, Value = Color.new(0, 1, 0) }, { Time = 1, Value = Color.new(0, 1, 1) } } }, other)
udc.createGradient({ Transparency = { { Time = 0, Value = 0 }, { Time = 1, Value = 1 } } }, fading)
```

`Color` takes a Color, a list of Colors spread evenly or keypoints, and a list of numbers or keypoints; both read back as keypoints sorted by `Time`. `Rotation` is in degrees, clockwise from left to right, and the gradient reaches the corners at any angle. `Offset` moves it by a fraction of the control's size, its end colors filling what it uncovers.

### Corners

Corners are rounded by a `Corner` instance placed in the control, instead of a property. Each corner has its own radius: `TopLeftRadius`, `TopRightRadius`, `BottomRightRadius` and `BottomLeftRadius`, and `CornerRadius` sets them all.

```luau
local button = udc.createTextButton({ Text = "Pill", Size = UDim2.fromOffset(100, 40) }, window)
udc.createCorner({ CornerRadius = UDim.new(0.5, 0) }, button)    -- half the shorter side: a pill
udc.createCorner({ CornerRadius = 0, TopLeftRadius = 12, TopRightRadius = 12 }, tab)    -- a tab's top
```

Each radius takes a UDim or a number of pixels, 8 pixels by default. Its scale is a fraction of the control's shorter side. When the radii along a side add up to more than the side, every radius is scaled down by the same factor, so the shape keeps its proportions.

### Fonts

Text controls have `Font` (a family name, empty for the platform's UI font), `TextSize` and `FontStyle`: `"Regular"`, `"Bold"`, `"Italic"` or `"BoldItalic"`.

`udc.loadFont(path)` loads a `.ttf` or `.otf` file for this process, after which its family name works in `Font` like an installed font:

```luau
udc.loadFont("assets/Bangers-Regular.ttf")
udc.createTextLabel({ Text = "Hello", Font = "Bangers", FontStyle = "Regular" }, window)
```

### Lines of text

By default, all text controls display one line. With `MultiLine` however, you can make the control display multiple lines of text.
Text controls also allow you to configure whether or not lines wider than the control wrap at their spaces, and a word too long for a line between its characters, using `TextWrapped`. And with `Truncated`, you can control whether or not a line wider than the control ends in an ellipsis ("…") where it's cut off.

### Text input

A `TextInput` draws a blue border while it's focused, `FocusedBorderColor` changes it, and `false` removes it, for inputs that show their focus another way. Its text sits within a `Padding` child's insets when it has one, and 6 pixels in from its sides otherwise.

With `MultiLine` or `TextWrapped`, a `TextInput` lays its text out on lines from its top, scrolled to keep the caret in view. With `MultiLine`, Enter types a newline instead of firing `OnSubmitted`, and pasting keeps every line.

A `TextInput` with `TextInputType = "Password"` works like a password box, always on one line: it shows a dot for each character and blocks copying and cutting, while `Text` and the `OnChanged`/`OnSubmitted`/`OnFocusLost` handlers still get the real text.

### Layouts

A `ListLayout` placed in a container arranges the container's other children:

| Property | Values | |
|---|---|---|
| `FillDirection` | `Vertical`, `Horizontal` | The direction items flow in |
| `HorizontalAlignment` | `Left`, `Center`, `Right` | |
| `VerticalAlignment` | `Top`, `Center`, `Bottom` | |
| `SortOrder` | `LayoutOrder`, `Name` | Uses each child's `LayoutOrder` or `Name` property |
| `Padding` | `UDim` or pixels | Between items and lines, the scale is a fraction of the container's length along `FillDirection` |
| `Wraps` | boolean | Starts a new line when an item doesn't fit |
| `HorizontalFlex`, `VerticalFlex` | `None`, `Fill`, `SpaceAround`, `SpaceBetween`, `SpaceEvenly` | How free space is shared out |
| `ItemLineAlignment` | `Automatic`, `Start`, `Center`, `End`, `Stretch` | How items sit across their line |
| `AbsoluteContentSize` | `vector`, read only | The size of the arranged items |

While it's there, the children's `Position` and `AnchorPoint` are ignored, their `Size` is still resolved against the container. Destroying it puts them back where they were. Inside a `ScrollView`, overflowing items can be scrolled to. `ScrollView.ScrollBarThickness` sets the scroll bars' width (0 hides them).

### High-DPI

Positions and sizes are logical pixels. On Windows the process is made per-monitor DPI aware (with fallbacks for older versions), and the drawn controls and the window's size are scaled by the monitor's scale, following the window when it moves to another monitor. macOS already works in points, so nothing changes there.

### Windows

`udc.createWindow` takes a `Title`, and optionally a `Name`, `Position` and `Size`, `Resizable` (true when left out, false removes the sizing border and the maximize button) and an `Icon`: an `.ico`, or an image GDI+ decodes, shown in the title bar and the taskbar at the window's DPI. On macOS windows have no icon of their own, so it becomes the application's icon in the Dock.

### Pointer events

Every drawn control takes `OnEnter`, `OnLeft`, `OnPressed` and `OnUnPressed` handlers in its descriptor. Each gets where the pointer is in the window (a `Vector2`, like `AbsolutePosition`) and its kind, `"Mouse"`, `"Touch"` or `"Pen"`. 

`OnEnter` and `OnLeft` follow hovering pointers over the top control under them; a pointer that doesn't hover, such as a touch, enters a control as it presses it and leaves as it lifts. `OnPressed` fires when a press goes down on a control and `OnUnPressed` when it comes back up, before a `TextButton`'s `OnActivated`.

`OnInputBegan` and `OnInputEnded` are the raw inputs, every button's press on a control and its release, wherever the pointer is by then, with an input table of `InputType` (`"MouseButton1"`, `"MouseButton2"`, `"MouseButton3"` or `"Touch"`), `Position` and `Pointer`.

Only active controls are hit. The top active control under a pointer gets its events, and one that isn't lets them through to what's behind it, so a label inside a button leaves the press to the button, and an empty overlay doesn't block what's under it. Buttons, inputs and scroll views are active, and so is a control given a pointer or input handler in its descriptor; `Active` overrides that. The wheel goes to whatever is under the pointer, active or not.

### Performance

The view lays out only after something changed, a control caches its frame until the next change, and text widths are cached by font, so painting and hit testing a tree of a hundred controls costs a fraction of a millisecond.

### Tree and property signals

Every gui object has a `Parent` and `FetchChildren()`. The controls at the top of a window give the window as their `Parent`, and a window's is always nil. Setting `Parent` moves a control and its descendants into another control or window, even another window; it can't be set to nil (`Destroy` removes a control) or to the control itself or one of its descendants.

`object:BindToPropertyChanged("Property")` returns a signal from the signal library, the same one each time for a property, fired without arguments when the value changes. Destroying an object disconnects its signals.

### Destroying

`window:Destroy()` destroys every control in the window, then closes it. `control:Destroy()` comes from BaseObject and the control's children are destroyed first, each destroying its own children, so the whole subtree goes in one chain. Their handlers are released, and every handle to them reports `Destroyed` and errors when used. A handler may destroy its own control, the control is released once the click is over, and the garbage collector frees it once scripts don't hold it either.

## With the GUI library

The [GUI](https://github.com/Luwow-Project/GUI) library makes native windows, buttons, menu bars and menu items, and UDC's controls can go in its windows beside them. Given a GUI window as their `Parent`, drawn controls are drawn into it:

```luau
local gui = require("@Luwow/gui")
local udc = require("@Luwow/udc")
local UDim2 = require("@Luwow/udim2")

local window = gui.createWindow({ Type = "Window", Title = "Mixed", Width = 400, Height = 300 })
gui.createButton({ Type = "Button", Caption = "Native", Left = 16, Top = 16, Width = 100, Height = 30, OnPressed = function() end }, window)

local label = udc.createTextLabel({ Parent = window, Text = "Drawn", Position = UDim2.fromOffset(16, 60), Size = UDim2.fromOffset(200, 30) })
print(label.Parent == udc.attach(window)) -- true
```

Using `udc.attach(window)`, the UDC library controls can be drawn into a window from the GUI library.
