-- This is the type system for the UDC (User-Drawn Controls) API as it exists in C++ code.

local baseobject = require("@Luwow/baseobject")
local color = require("@Luwow/color")
local rect = require("@Luwow/rect")
local signal = require("@Luwow/signal")
local udim = require("@Luwow/udim")
local udim2 = require("@Luwow/udim2")

--[=[
    UDC uses other libraries as dependencies, including their types.
]=]
export type BaseObject = baseobject.BaseObject
export type Color = color.Color
export type Rect = rect.Rect
export type UDim = udim.UDim
export type UDimLike = udim.UDimLike
export type UDim2 = udim2.UDim2
export type UDim2Like = udim2.UDim2Like
export type Signal<T...> = signal.Signal<T...>

--[=[
	The types for each native component object.
]=]

--[=[
    User drawn control class inheritance tree:

	BaseObject
	└─ BaseGuiObject
	   ├─ Window
	   ├─ ListLayout
	   ├─ Corner
	   ├─ Padding
	   ├─ Stroke
	   ├─ Gradient
	   └─ ControlObject
	      ├─ View
	      ├─ ScrollView
	      ├─ ImageObject
	      │  ├─ ImageLabel
	      │  └─ ImageButton
	      └─ TextObject
	         ├─ TextLabel
	         ├─ TextButton
	         └─ TextInput
]=]

--[=[
	A window is the top of an UI tree: its drawn controls are its children, and its Parent is always nil.
]=]
export type Window = BaseGuiObject & {
    read ClassName: "Window",
    -- The title of the window.
    Title: string,
    -- The window this one belongs to, nil for none.
    Owner: Window?,
    -- Whether the window lets the input pass to the owner window.
    Modal: boolean,
    -- Whether the window takes input.
    Enabled: boolean,
    -- Whether the window stays above the windows of other programs.
    TopMost: boolean,
    -- Whether the close button only fires OnCloseRequested, leaving the closing to the script.
    PreventClose: boolean,
    -- The size of the area inside the frame, where the controls are.
    ClientSize: vector,
    read Visible: boolean,

    --[=[
		Shows the window.
	]=]
    Show: (self: Window) -> (),

    --[=[
		Hides the window, it can be shown again.
	]=]
    Hide: (self: Window) -> (),

    --[=[
		Closes the native window and fires OnClosed. The controls are kept until it's destroyed.
	]=]
    Close: (self: Window) -> (),

    --[=[
		Brings the window to the front and gives it the keyboard.
	]=]
    Focus: (self: Window) -> (),

    --[=[
		Moves the window to the middle of its Owner, or of the screen when there's none or it's hidden.
	]=]
    Center: (self: Window) -> (),

    --[=[
		Puts the text on the system clipboard.
	]=]
    SetClipboardText: (self: Window, text: string) -> ()
}

--[=[
	Common properties every gui object contains.
]=]
export type BaseGuiObject = BaseObject & {
    -- The Window or ControlObject it's in, nil for a window. Setting it moves the object and its
    -- descendants to another window. It can't be set to nil.
    Parent: Parent?,

    --[=[
		Returns the objects parented to this one, in the order they were added, helpers such as a
		ListLayout included.
	]=]
    FetchChildren: (self: BaseGuiObject) -> { BaseGuiObject },

    --[=[
		Returns a signal fired when the property's value changes, the same one each time for a
		property. It fires without arguments, read the property for the new value.

		@param propertyName The property to watch, raising for unknown properties and methods.
	]=]
    BindToPropertyChanged: (self: BaseGuiObject, propertyName: string) -> Signal<nil>,
}

export type TextAlignment = "Left" | "Center" | "Right"
export type ScaleType = "Stretch" | "Fit" | "Crop" | "Slice" | "Tile"
export type FontStyle = "Regular" | "Bold" | "Italic" | "BoldItalic"
export type TextInputType = "Default" | "Password"
export type FillDirection = "Vertical" | "Horizontal"
export type HorizontalAlignment = "Left" | "Center" | "Right"
export type VerticalAlignment = "Top" | "Center" | "Bottom"
export type SortOrder = "LayoutOrder" | "Name"
export type FlexAlignment = "None" | "Fill" | "SpaceAround" | "SpaceBetween" | "SpaceEvenly"
export type ItemLineAlignment = "Automatic" | "Start" | "Center" | "End" | "Stretch"
export type FlexMode = "None" | "Grow" | "Shrink" | "Fill" | "Custom"
-- The axes a size grows along to fit content
export type AutomaticSize = "None" | "X" | "Y" | "XY"
export type PointerKind = "Mouse" | "Touch" | "Pen"

--[=[
	How a control grows into and shrinks out of its line in a ListLayout.
	Grow takes free space, Shrink gives space back when the line overflows, and Fill does both. Free
	space is shared in proportion to the grow ratios, overflow in proportion to the shrink ratios times
	the sizes. Custom uses the ratios given; ratios without a Mode are Custom. ItemLineAlignment wins
	over the layout's for this control.

	A control's Flex can be set to a FlexMode name too, such as "Grow". It reads back as a frozen table
	with every field, the ratios being the ones the mode uses.
]=]
export type Flex = {
    Mode: FlexMode?,
    GrowRatio: number?,
    ShrinkRatio: number?,
    ItemLineAlignment: ItemLineAlignment?
}

--[=[
	A handler for a pointer event, where the pointer is in the window, in pixels like AbsolutePosition,
	and what kind of pointer it is.
]=]
export type PointerHandler = (position: vector, pointer: PointerKind) -> ()

--[=[
	An input, which button or touch, where it is in the window in
	pixels like AbsolutePosition, and what kind of pointer it is.
]=]
export type InputType = "MouseButton1" | "MouseButton2" | "MouseButton3" | "Touch"
export type Input = {
    InputType: InputType,
    Position: vector,
    Pointer: PointerKind
}
export type InputHandler = (input: Input) -> ()

--[=[
	A control that's drawn, with a position, a size and colors.
]=]
export type ControlObject = BaseGuiObject & {
    -- Relative to the parent's content, inside its border.
    Position: UDim2,
    -- 100% of the parent by default.
    Size: UDim2,
    -- The point of the control placed at its Position, from (0, 0) at the top left to (1, 1) at the
    -- bottom right. vector.create(0.5, 0.5) centers it.
    AnchorPoint: vector,
    -- Limits on the size. MinSize wins when they conflict.
    MinSize: UDim2?,
    MaxSize: UDim2?,
    -- Width over height, the size is fitted within the
    -- resolved size. nil doesn't constrain it.
    AspectRatio: number?,
    -- How a ListLayout parent grows and shrinks the control. nil keeps its size.
    Flex: Flex?,
    -- Grows the size along these axes to fit the content, never shrinking it below Size.
    AutomaticSize: AutomaticSize,
    Visible: boolean,
    -- Whether the children are cut off at the control's edges, and can only be hit within them.
    ClipsDescendants: boolean,
    -- Whether pointers hit the control, the top active control under a pointer gets
    -- its events, and inactive ones let them through to what's behind them, such as a label inside a button.
    Active: boolean,
    -- Orders the control among its siblings in a ListLayout.
    LayoutOrder: number,
    -- The order among siblings, 1 by default: higher draws on top and gets the mouse first. Equal ones
    -- keep the order they were created in.
    ZIndex: number,
    -- nil draws no background or border.
    BackgroundColor: Color?,
    BorderColor: Color?,
    BorderWidth: number,
    -- Where the control ends up in the window in pixels, after layouts and scrolling.
    read AbsolutePosition: vector,
    read AbsoluteSize: vector,
    read AbsoluteFrame: Rect
}

--[=[
	A control showing text.
]=]
export type TextObject = ControlObject & {
    Text: string,
    TextColor: Color,
    -- The text height in pixels, 14 by default.
    TextSize: number,
    -- A font family name: installed, or loaded with udc.loadFont. Empty uses the platform's UI font.
    Font: string,
    FontStyle: FontStyle,
    TextAlignment: TextAlignment,
    -- Newlines start new lines, and Enter types one in a TextInput instead of firing OnSubmitted. false by default.
    MultiLine: boolean,
    -- Lines wider than the control wrap. With AutomaticSize along X the control grows instead, and the
    -- text only wraps at its MaxSize width. false by default.
    TextWrapped: boolean,
    -- A line wider than the control is cut off with an ellipsis.
    Truncated: boolean
}

export type View = ControlObject & {
    read ClassName: "View"
}

export type ScrollView = ControlObject & {
    read ClassName: "ScrollView",
    -- How far the content is scrolled in pixels, kept within the content.
    ScrollPosition: vector,
    -- The scrollable size in pixels. A component of 0 measures the children.
    ContentSize: vector,
    -- Grows the scrollable size along these axes to fit the children, never below ContentSize. "None" by default.
    AutomaticCanvasSize: AutomaticSize,
    ScrollBarColor: Color,
    -- The width of the scroll bars. 0 hides them, the wheel still scrolls.
    ScrollBarThickness: number
}

--[=[
	A control showing an image over its background.
]=]
export type ImageObject = ControlObject & {
    -- An image file, PNG, JPEG, BMP or GIF.
    Image: string | buffer,
    -- Stretch fills the control, Fit shows the whole image keeping its proportions, Crop fills the
    -- control keeping its proportions and cuts off what doesn't fit. Slice keeps the borders around
    -- SliceCenter at their size, and Tile repeats the image at TileSize.
    ScaleType: ScaleType,
    -- The image's size in pixels, 0, 0 when there's none or it couldn't be loaded.
    read ImageSize: vector,
    -- The part of the image shown, in its pixels, like a sprite sheet's icon. A size of 0, 0 shows all of it.
    ImageRectOffset: vector,
    ImageRectSize: vector,
    -- Degrees the image turns clockwise about its center, 0 by default. Slice and Tile images don't turn, and
    -- neither do the control and its children.
    Rotation: number,
    ImageColor: Color,
    -- The stretched middle of the shown part, in its pixels. The borders around it keep their size
    -- times SliceScale, shrinking together when they don't fit. An empty Rect stretches the whole image.
    SliceCenter: Rect,
    SliceScale: number,
    -- Each tile's size, its scale a fraction of the control's content. The whole content by default.
    TileSize: UDim2
}

export type ImageLabel = ImageObject & {
    read ClassName: "ImageLabel"
}

--[=[
	A button showing an image.
]=]
export type ImageButton = ImageObject & {
    read ClassName: "ImageButton",
    -- Empty keeps Image.
    HoverImage: string | buffer,
    PressedImage: string | buffer,
    -- nil keeps BackgroundColor.
    HoverColor: Color?,
    PressedColor: Color?
}

export type TextLabel = TextObject & {
    read ClassName: "TextLabel"
}

export type TextButton = TextObject & {
    read ClassName: "TextButton",
    HoverColor: Color,
    PressedColor: Color
}

export type TextInput = TextObject & {
    read ClassName: "TextInput",
    PlaceholderText: string,
    PlaceholderColor: Color,
    SelectionColor: Color,
    -- The border while the input is focused, blue by default. false draws none.
    FocusedBorderColor: Color | false,
    -- "Password" shows a dot for each character, and blocks copying and cutting. Text is still the real text.
    TextInputType: TextInputType,
    read Focused: boolean,

    --[=[
		Gives the input the keyboard focus.
	]=]
    Focus: (self: TextInput) -> (),

    --[=[
		Takes the keyboard focus away, if the input has it.
	]=]
    ReleaseFocus: (self: TextInput) -> ()
}

--[=[
	Arranges its parent's other children. It isn't drawn, so it isn't
	a ControlObject.
]=]
export type ListLayout = BaseGuiObject & {
    read ClassName: "ListLayout",
    FillDirection: FillDirection,
    HorizontalAlignment: HorizontalAlignment,
    VerticalAlignment: VerticalAlignment,
    SortOrder: SortOrder,
    -- Space between items, and between lines when wrapping. Its scale is a fraction of the container's
    -- length along FillDirection.
    Padding: UDim,
    -- Starts a new line when the next item doesn't fit.
    Wraps: boolean,
    -- How free space is shared out along each axis. Fill grows the items to use it.
    HorizontalFlex: FlexAlignment,
    VerticalFlex: FlexAlignment,
    -- How items sit across their line. Automatic follows the alignment, Stretch fills the line.
    ItemLineAlignment: ItemLineAlignment,
    -- The size of the arranged children in pixels.
    read AbsoluteContentSize: vector
}

--[=[
	Rounds its parent's corners, each corner has its own radius.
]=]
export type Corner = BaseGuiObject & {
    read ClassName: "Corner",
    -- Sets every corner. It reads back as TopLeftRadius, the others may differ.
    CornerRadius: UDim,
    TopLeftRadius: UDim,
    TopRightRadius: UDim,
    BottomRightRadius: UDim,
    BottomLeftRadius: UDim
}

--[=[
	Insets its parent's content, the parent's children are placed and sized
	within the padded area, and layouts arrange them there.
]=]
export type Padding = BaseGuiObject & {
    read ClassName: "Padding",
    PaddingTop: UDim,
    PaddingBottom: UDim,
    PaddingLeft: UDim,
    PaddingRight: UDim
}

--[=[
	Outlines its parent, a line along the parent's frame, outside it by default,
	following its rounded corners, over its background and under its children.
]=]
export type Stroke = BaseGuiObject & {
    read ClassName: "Stroke",
    -- Black by default.
    Color: Color,
    -- In whole pixels, 1 by default. 0 draws nothing.
    Thickness: number,
    -- Where the line goes, outside the frame, across its edge, or
    -- inside it. "Outer" by default.
    StrokePosition: StrokePosition
}

export type StrokePosition = "Outer" | "Center" | "Inner"

--[=[
	A value at a point along a gradient, from 0 at its start to 1 at its end.
]=]
export type Keypoint<T> = {
    Time: number,
    Value: T
}

--[=[
	Colors its parent's background along a line, at each point the background's
	color is multiplied by the gradient's, and its opacity by the gradient's.

    Color can be set to a Color, or a list of keypoints.
	They read back as keypoints, sorted by Time.
]=]
export type Gradient = BaseGuiObject & {
    read ClassName: "Gradient",
    -- White by default.
    Color: { Keypoint<Color> } | Color,
    -- 0 by default.
    Transparency: { Keypoint<number> } | number,
    -- Degrees, clockwise from left to right.
    Rotation: number,
    -- Moves the gradient by a fraction of the parent's size, its ends' colors filling what it uncovers.
    Offset: vector,
    Enabled: boolean
}

--[=[
	The descriptor types for each native component.
]=]

export type WindowDescriptor = {
    Title: string,
    Name: string?,
    -- Where the window opens on the screen.
    Position: vector?,
    Size: vector?,
    -- Whether the user can resize and maximize the window, true when left out.
    Resizable: boolean?,
    -- An .ico, or a PNG, JPEG, BMP or GIF
    Icon: string?,
    -- The size of the area inside the frame, where the controls are. Given, it decides the window's
    -- size instead of Size.
    ClientSize: vector?,
    -- Opens in the middle of its Owner, or of the screen without one, instead of at Position.
    Centered: boolean?,
    -- Whether it can be minimized, true when left out.
    Minimizable: boolean?,
    TopMost: boolean?,
    -- The owner of the Window, which can be another Window.
    Owner: Window?,
    -- Whether the window lets the input pass to the owner window.
    Modal: boolean?,
    PreventClose: boolean?,
    -- Called when the user asks to close the window, such as with its close button. It closes
    -- afterwards unless PreventClose is set.
    OnCloseRequested: (() -> ())?,
    -- Called once the window is closed, by the user or by Close. Destroy doesn't call it.
    OnClosed: (() -> ())?,
    -- Called when the window becomes the active one, such as when it's clicked or shown.
    OnFocused: (() -> ())?
}

export type BaseObjectDescriptor = {
    Parent: Parent?,
    Name: string?
}

export type ControlObjectDescriptor = BaseObjectDescriptor & {
    Position: UDim2Like?,
    Size: UDim2Like?,
    AnchorPoint: vector?,
    MinSize: UDim2Like?,
    MaxSize: UDim2Like?,
    AspectRatio: number?,
    Flex: (Flex | FlexMode)?,
    Visible: boolean?,
    ClipsDescendants: boolean?,
    Active: boolean?,
    LayoutOrder: number?,
    ZIndex: number?,
    -- false removes the control's default color.
    BackgroundColor: (Color | false)?,
    BorderColor: (Color | false)?,
    BorderWidth: number?,
    AutomaticSize: AutomaticSize?,

    --[=[
		OnEnter and OnLeft fire as a hovering pointer (a mouse or a pen) moves over and off the control,
		the top one under it. A pointer that doesn't hover, such as a touch, enters as it presses and
		leaves as it lifts.

		OnPressed fires when a press goes down on the control, and OnUnPressed when it comes back up,
		wherever the pointer is by then, before a TextButton's OnActivated.
	]=]
    OnEnter: PointerHandler?,
    OnLeft: PointerHandler?,
    OnPressed: PointerHandler?,
    OnUnPressed: PointerHandler?,

    --[=[
		Inputs, any pointer button (left, right or middle), a
		touch or a pen going down on the control, and coming back up, wherever the pointer is by then.
		Unlike OnPressed, every button fires them.
	]=]
    OnInputBegan: InputHandler?,
    OnInputEnded: InputHandler?
}

export type TextObjectDescriptor = ControlObjectDescriptor & {
    MultiLine: boolean?,
    TextWrapped: boolean?,
    Truncated: boolean?,
    Text: string?,
    TextColor: Color?,
    TextSize: number?,
    Font: string?,
    FontStyle: FontStyle?,
    TextAlignment: TextAlignment?
}

export type ViewDescriptor = ControlObjectDescriptor

export type ScrollViewDescriptor = ControlObjectDescriptor & {
    ContentSize: vector?,
    AutomaticCanvasSize: AutomaticSize?,
    ScrollBarColor: Color?,
    ScrollBarThickness: number?
}

export type ImageObjectDescriptor = ControlObjectDescriptor & {
    Image: string?,
    ScaleType: ScaleType?,
    ImageRectOffset: vector?,
    ImageRectSize: vector?,
    Rotation: number?,
    ImageColor: Color?,
    SliceCenter: Rect?,
    SliceScale: number?,
    TileSize: UDim2Like?
}

export type ImageLabelDescriptor = ImageObjectDescriptor

export type ImageButtonDescriptor = ImageObjectDescriptor & {
    HoverImage: string?,
    PressedImage: string?,
    HoverColor: Color?,
    PressedColor: Color?,
    -- Runs when the button is clicked. It runs in its own coroutine, so it may yield.
    OnActivated: (() -> ())?
}

export type TextLabelDescriptor = TextObjectDescriptor

export type TextButtonDescriptor = TextObjectDescriptor & {
    HoverColor: Color?,
    PressedColor: Color?,
    -- Runs when the button is clicked. It runs in its own coroutine, so it may yield.
    OnActivated: (() -> ())?
}

export type TextInputDescriptor = TextObjectDescriptor & {
    PlaceholderText: string?,
    PlaceholderColor: Color?,
    SelectionColor: Color?,
    FocusedBorderColor: (Color | false)?,
    TextInputType: TextInputType?,
    -- Runs after every edit made by the user, with the new text.
    OnChanged: ((text: string) -> ())?,
    -- Runs when Enter is pressed.
    OnSubmitted: ((text: string) -> ())?,
    -- Runs when the input stops being focused.
    OnFocusLost: ((text: string) -> ())?
}

export type ListLayoutDescriptor = BaseObjectDescriptor & {
    FillDirection: FillDirection?,
    HorizontalAlignment: HorizontalAlignment?,
    VerticalAlignment: VerticalAlignment?,
    SortOrder: SortOrder?,
    Padding: UDimLike?,
    Wraps: boolean?,
    HorizontalFlex: FlexAlignment?,
    VerticalFlex: FlexAlignment?,
    ItemLineAlignment: ItemLineAlignment?,
}

export type CornerDescriptor = BaseObjectDescriptor & {
    CornerRadius: UDimLike?,
    TopLeftRadius: UDimLike?,
    TopRightRadius: UDimLike?,
    BottomRightRadius: UDimLike?,
    BottomLeftRadius: UDimLike?
}

export type PaddingDescriptor = BaseObjectDescriptor & {
    PaddingTop: UDimLike?,
    PaddingBottom: UDimLike?,
    PaddingLeft: UDimLike?,
    PaddingRight: UDimLike?
}

export type GradientDescriptor = BaseObjectDescriptor & {
    Color: (Color | { Color } | { Keypoint<Color> })?,
    Transparency: (number | { number } | { Keypoint<number> })?,
    Rotation: number?,
    Offset: vector?,
    Enabled: boolean?
}

export type StrokeDescriptor = BaseObjectDescriptor & {
    Color: Color?,
    Thickness: number?,
    StrokePosition: StrokePosition?
}

export type Parent = Window | ControlObject

export type Udc = {
    --[=[
		Creates a new Window with the given descriptor. Windows belong to the main thread. From a
		coroutine on another thread, the coroutine yields until the main thread has made it.

		@param descriptor The window descriptor that defines the properties of the window.
		@return The new Window object.
	]=]
    createWindow: (descriptor: WindowDescriptor) -> Window,

    --[=[
		The Window drawing into a gui library's window, made the first time and the same one after.
		Controls given the native window as their Parent have it as their Parent. Main thread only.

		@param window A window from gui.
		@return The UDC Window drawing into it.
	]=]
    attach: (window: any) -> Window,

    --[=[
		Plays the system's sound for a message of the kind, the one dialogs play.
	]=]
    beep: (kind: ("Information" | "Warning" | "Error" | "Default")?) -> (),

    --[=[
		Creates a View, a plain container with an optional background and border.

		@param descriptor The view's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createView: (descriptor: ViewDescriptor, parent: Parent?) -> View,

    --[=[
		Creates a ScrollView, a container scrolled with the mouse wheel (Shift for sideways) or its scroll bars.

		@param descriptor The scroll view's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createScrollView: (descriptor: ScrollViewDescriptor, parent: Parent?) -> ScrollView,

    --[=[
		Creates a TextLabel, text on one line, or several with MultiLine or TextWrapped.

		@param descriptor The label's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createTextLabel: (descriptor: TextLabelDescriptor, parent: Parent?) -> TextLabel,

    --[=[
		Creates an ImageLabel, an image from a file or a buffer.

		@param descriptor The label's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createImageLabel: (descriptor: ImageLabelDescriptor, parent: Parent?) -> ImageLabel,

    --[=[
		Creates a TextButton, a drawn button with text.

		@param descriptor The button's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createTextButton: (descriptor: TextButtonDescriptor, parent: Parent?) -> TextButton,

    --[=[
		Creates a TextInput, a text box.

		@param descriptor The input's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createTextInput: (descriptor: TextInputDescriptor, parent: Parent?) -> TextInput,

    --[=[
		Creates a ListLayout, which arranges its parent's other children in a row or column. 
        While it's there, its siblings' Position and AnchorPoint are ignored,
		their Size is still resolved against the parent. Destroying it gives them back their own
		positions.

		@param descriptor The layout's properties.
		@param parent The Window or drawn control whose children it arranges.
		@return The new control.
	]=]
    createListLayout: (descriptor: ListLayoutDescriptor, parent: Parent?) -> ListLayout,

    --[=[
		Creates a Corner, which rounds its parent's corners.

		@param descriptor The corner's properties.
		@param parent The drawn control to round.
		@return The new control.
	]=]
    createCorner: (descriptor: CornerDescriptor, parent: ControlObject?) -> Corner,

    --[=[
		Creates a Padding, which insets its parent's content.

		@param descriptor The padding's properties.
		@param parent The drawn control to pad.
		@return The new padding.
	]=]
    createPadding: (descriptor: PaddingDescriptor, parent: ControlObject?) -> Padding,

    --[=[
		Creates a Stroke, which outlines its parent.

		@param descriptor The stroke's properties.
		@param parent The drawn control to outline.
		@return The new stroke.
	]=]
    createStroke: (descriptor: StrokeDescriptor, parent: ControlObject?) -> Stroke,

    --[=[
		Creates an ImageButton, an image that's clicked like a TextButton.

		@param descriptor The button's properties.
		@param parent The Window or drawn control to put it in, or the descriptor's Parent.
		@return The new control.
	]=]
    createImageButton: (descriptor: ImageButtonDescriptor, parent: Parent?) -> ImageButton,

    --[=[
		Creates a Gradient, which colors its parent's background along a line.

		@param descriptor The gradient's properties.
		@param parent The drawn control whose background it colors.
		@return The new gradient.
	]=]
    createGradient: (descriptor: GradientDescriptor, parent: ControlObject?) -> Gradient,

    --[=[
		Loads a font file (.ttf or .otf) for this process, so its family name works in Font like an
		installed font. Relative paths are from the working directory. Raises an error if the file can't
		be loaded.

		@param path The font file.
	]=]
    loadFont: (path: string) -> (),
}

return table.freeze({} :: Udc)
