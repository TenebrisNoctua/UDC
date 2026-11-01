#pragma once

#include "ControlObject.h"
#include "Input.h"
#include "Scaling.h"
#include "ILuauHost.h"

#include <functional>
#include <memory>
#include <unordered_set>

struct lua_State;

namespace Luwow::Engine {
class ILuauHost;
}

namespace Luwow::Udc {
    // Owns a window's drawn controls: paints them, routes the window's input to them, and runs their
    // Luau handlers.
    class ControlView {
    public:
        ControlView(IControlSurface* surface, Luwow::Engine::ILuauHost* luauHost);
        ~ControlView();

        IControlSurface* const surface;
        Luwow::Engine::ILuauHost* const luauHost;

        // The invisible control filling the window, parent of the top level controls
        ControlObject& root() { return *rootControl; }

        // The window's Luau object, what the top level controls give as their Parent
        BaseGuiObject* owner = nullptr;

        // Which inputs fire the controls' pointer events, the defaults for the system's input hardware
        InputBindings bindings;

        // Adds a control under parent and keeps its userdata alive, returns its id
        uint64_t add(ControlObject* control, ControlObject* parent, lua_State* L);

        // The control with the id, nullptr once it's destroyed
        ControlObject* find(uint64_t id) const;

        void destroy(ControlObject* control, lua_State* L);
        // Destroys every control, when the window is destroyed
        void destroyAll(lua_State* L);

        // Moves a control and its descendants under parent, which may be in another window's view.
        // Raises when parent is the control or one of its descendants.
        void move(ControlObject* control, ControlObject* parent, lua_State* L);

        // Checks the control's bound properties after each paint and input event
        void watch(ControlObject& control);
        void setFocus(ControlObject* control);
        bool isWindowFocused() const { return windowFocused; }

        // The modifiers held during the last key or mouse press
        KeyModifiers modifiers() const { return currentModifiers; }

        // Asks for a repaint, and for the layout to run again first
        void invalidate();
        // Runs the layout and fires the watched controls' signals without painting, for windows that
        // can't paint, such as hidden or minimized ones
        void settle();

        // Frames computed before this changes are out of date. It changes with anything that can move or
        // resize a control, so controls cache their frames by it.
        uint64_t frameGeneration() const { return generation; }
        // Whether the layout is running, when frames aren't final yet and aren't cached
        bool isLayingOut() const { return layingOut; }
        // Drops the cached frames without asking for a layout, for changes made while painting
        void invalidateFrames() { ++generation; }

        // Logical pixels per device pixel conversions for the window's current DPI
        Scaler scaler() const { return Scaler(surface->scale()); }

        // Text width in logical pixels
        int measureText(const std::string& text, const Font& font);

        // The image a control shows, loaded once and shared by every control showing it. nullptr if it can't
        // be loaded, which is cached too, so a missing file isn't read again on every paint.
        std::shared_ptr<ICanvasImage> image(const ImageSource& source);

        // Sizes the root to the window and runs the layout controllers, before anything reads frames. Does
        // nothing when nothing changed since the last time.
        void update();

        // Runs the control's handler for event in its own coroutine, through the scheduler when there is one.
        // pushArgs pushes the arguments onto the coroutine and returns how many. The control may be
        // destroyed by the handler, so callers shouldn't use it afterwards.
        void fireEvent(ControlObject& control, const char* event, const std::function<int(lua_State*)>& pushArgs = nullptr);

        // Called by the platform window before it starts a paint, so what the watched signals' handlers change
        // is painted with it instead of asking for another paint
        void preparePaint();
        // Called by the platform window. The canvas and positions are in device pixels, wheel amounts are logical.
        void paint(ICanvas& canvas);
        void pointerDown(const PointerInput& input);
        void pointerMove(const PointerInput& input);
        void pointerUp(const PointerInput& input);
        // The pointer left the window
        void pointerLeave();
        void wheel(int x, int y, int dx, int dy);
        void key(Key key, KeyModifiers modifiers);
        void text(const std::string& text);
        void setWindowFocused(bool focused);

    private:
        // Clears the layout overrides under control, then lets its layout controller set them again
        void layout(ControlObject& control);
        void paintControl(ICanvas& canvas, ControlObject& control);
        // Moves the hover to target, firing OnLeft and OnEnter. Positions are logical.
        void setHovered(ControlObject* target, int x, int y, PointerKind kind);
        // The deepest visible control at the point that's active, or any one without activeOnly. nullptr for
        // none, the root counts as none.
        ControlObject* hitTest(int x, int y, bool activeOnly = true);
        ControlObject* hitTest(ControlObject& control, int x, int y, bool activeOnly);
        // Every control under the point, front to back: each one's children before it
        void controlsAt(ControlObject& control, int x, int y, std::vector<ControlObject*>& found);
        // Clears every reference the view keeps to the control, but not its children
        void release(ControlObject& control, lua_State* L);
        // Hands a control and its descendants to another view, with new ids there
        void transfer(ControlObject& control, ControlView& target);
        // Fires the signals of the watched controls' changed properties
        void checkWatched();
        // Releases destroyed controls' userdata, so nothing should use them anymore.
        void collect();

        // Taken by the paint and input entry points. Holds the state lock, and releases destroyed controls
        // on the way out of the outermost one
        class Entry {
        public:
            explicit Entry(ControlView& view);
            ~Entry();

        private:
            ControlView& view;
            Luwow::Engine::StateLock lock;
        };

        std::unique_ptr<ControlObject> rootControl;
        std::unordered_map<uint64_t, ControlObject*> controls;
        std::unordered_map<std::string, std::shared_ptr<ICanvasImage>> images;
        // By the pixels they were made from, until no control has those pixels anymore
        std::unordered_map<const PixelData*, std::pair<std::weak_ptr<const PixelData>, std::shared_ptr<ICanvasImage>>> pixelImages;
        // Text widths by font and text, measuring through the platform is slow
        std::unordered_map<std::string, int> textWidths;
        float textWidthsScale = 0;
        uint64_t generation = 1;
        bool layoutDirty = true;
        bool layingOut = false;
        Rect laidOutBounds{ 0, 0, -1, -1 };
        uint64_t nextId = 1;
        std::vector<ControlObject*> destroyed;
        // Ids of the controls with property signals
        std::unordered_set<uint64_t> watched;
        int entryDepth = 0;
        bool invalidatedInEntry = false;
        bool watchPending = false;
        void checkPendingWatched();

        ControlObject* hoveredControl = nullptr;
        ControlObject* pressedControl = nullptr;
        // The press pressedControl got: its button, and whether the bindings made it fire OnPressed
        PointerButton pressedButton = PointerButton::None;
        bool pressedBound = false;
        // The last pointer position, logical, and its kind
        int pointerX = 0;
        int pointerY = 0;
        PointerKind pointerKind = PointerKind::Mouse;
        ControlObject* focusedControl = nullptr;
        // The control each button's press went to, for its OnInputEnded, by PointerButton
        uint64_t inputBeganIds[4] = {};
        bool windowFocused = true;
        KeyModifiers currentModifiers;
    };
} // namespace Luwow::Udc
