#include "ControlView.h"
#include "ListLayout.h"
#include "Stroke.h"
#include "View.h"
#include "ILuauHost.h"

#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <iterator>
#include <cstdio>

namespace Luwow::Udc {
    using Message = Luwow::Engine::Message;
    namespace Topics = Luwow::Engine::Topics;

    ControlView::ControlView(IControlSurface* surface, Luwow::Engine::ILuauHost* luauHost)
        : surface(surface), luauHost(luauHost), bindings(InputBindings::defaults(inputCapabilities())),
          rootControl(std::make_unique<View>()) {
        rootControl->view = this;
    }

    ControlView::~ControlView() = default;

    uint64_t ControlView::add(ControlObject* control, ControlObject* parent, lua_State* L) {
        control->id = nextId++;
        control->view = this;
        control->parent = parent;
        parent->children.push_back(control);
        control->retain(L);

        controls[control->id] = control;
        invalidate();
        return control->id;
    }

    ControlObject* ControlView::find(uint64_t id) const {
        auto it = controls.find(id);
        return it == controls.end() ? nullptr : it->second;
    }

    ControlView::Entry::Entry(ControlView& view) : view(view), lock(view.luauHost) {
        ++view.entryDepth;
    }

    // Changes are looked for once the paint or input is done, so no handler runs in the middle of it
    ControlView::Entry::~Entry() {
        if (view.entryDepth == 1) view.checkWatched();
        if (--view.entryDepth == 0) view.collect();
    }

    void ControlView::watch(ControlObject& control) {
        if (find(control.id) == &control) watched.insert(control.id);
    }

    void ControlView::checkWatched() {
        if (watched.empty()) return;
        lua_State* L = luauHost->getMainState();
        std::vector<uint64_t> ids(watched.begin(), watched.end());
        for (uint64_t id : ids) {
            if (ControlObject* control = find(id)) control->checkPropertiesChanged(L);
        }
    }

    void ControlView::collect() {
        if (destroyed.empty()) return;
        lua_State* L = luauHost->getMainState();
        for (ControlObject* control : destroyed) control->release(L);
        destroyed.clear();
    }

    void ControlView::release(ControlObject& control, lua_State* L) {
        for (auto& [event, ref] : control.handlers) lua_unref(L, ref);
        control.handlers.clear();
        control.releasePropertySignals(L);
        if (control.flexTable != LUA_NOREF) lua_unref(L, control.flexTable);
        control.flexTable = LUA_NOREF;
        controls.erase(control.id);
        watched.erase(control.id);

        if (hoveredControl == &control) hoveredControl = nullptr;
        if (pressedControl == &control) {
            pressedControl = nullptr;
            surface->captureMouse(false);
        }
        if (focusedControl == &control) focusedControl = nullptr;
    }

    void ControlView::destroy(ControlObject* control, lua_State* L) {
        if (!control || control == rootControl.get() || find(control->id) != control) return;

        // Each child destroys its own children before going, down to the last one
        while (!control->children.empty()) destroy(control->children.back(), L);

        release(*control, L);

        auto& siblings = control->parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), control), siblings.end());
        destroyed.push_back(control);
        control->parent = nullptr;
        control->BaseObject::destroy(L);

        // Outside a paint or input event no code of the control can be running, so it's released now
        if (entryDepth == 0) collect();

        invalidate();
    }

    void ControlView::destroyAll(lua_State* L) {
        while (!rootControl->children.empty()) destroy(rootControl->children.back(), L);
    }

    void ControlView::move(ControlObject* control, ControlObject* parent, lua_State* L) {
        if (control->parent == parent) return;
        for (ControlObject* ancestor = parent; ancestor; ancestor = ancestor->parent) {
            if (ancestor == control) luaL_error(L, "a %s can't be put inside itself or its descendants", control->getClassName());
        }

        ControlView* target = parent->view;
        if (target != this) {
            // The focus can't follow it to another window. OnFocusLost may run Luau that changes the tree.
            for (ControlObject* focused = focusedControl; focused; focused = focused->parent) {
                if (focused != control) continue;
                uint64_t id = control->id;
                setFocus(nullptr);
                if (find(id) != control) return;
                break;
            }
        }

        auto& siblings = control->parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), control), siblings.end());
        if (target != this) transfer(*control, *target);
        control->parent = parent;
        parent->children.push_back(control);

        invalidate();
        target->invalidate();
    }

    void ControlView::transfer(ControlObject& control, ControlView& target) {
        for (ControlObject* child : control.children) transfer(*child, target);

        if (hoveredControl == &control) {
            hoveredControl = nullptr;
            control.hovered = false;
        }
        if (pressedControl == &control) {
            pressedControl = nullptr;
            control.pressed = false;
            surface->captureMouse(false);
        }

        controls.erase(control.id);
        bool wasWatched = watched.erase(control.id) > 0;

        control.id = target.nextId++;
        control.view = &target;
        target.controls[control.id] = &control;
        if (wasWatched) target.watched.insert(control.id);
    }

    // The old control is told first, its OnFocusLost may run Luau that changes the tree
    void ControlView::setFocus(ControlObject* control) {
        if (focusedControl == control) return;

        ControlObject* old = focusedControl;
        focusedControl = control;

        uint64_t newId = control ? control->id : 0;
        if (old) {
            old->focused = false;
            old->onFocusChanged();
        }

        if (control && find(newId) == control && focusedControl == control) {
            control->focused = true;
            control->onFocusChanged();
        }
    }

    void ControlView::fireEvent(ControlObject& control, const char* event, const std::function<int(lua_State*)>& pushArgs) {
        auto handler = control.handlers.find(event);
        if (handler == control.handlers.end()) return;

        Luwow::Engine::StateLock lock(luauHost);
        lua_State* L = luauHost->getMainState();

        lua_State* thread = lua_newthread(L);
        lua_checkstack(thread, 8);
        lua_getref(L, handler->second);
        lua_xmove(L, thread, 1);
        int nargs = pushArgs ? pushArgs(thread) : 0;

        // Through the scheduler, the handler can yield. Without one it runs directly.
        Message request;
        request.topic = Topics::SchedulerResume;
        request.state = thread;
        request.result = nargs;
        if (!luauHost->request(request)) {
            int status = lua_resume(thread, L, nargs);
            if (status != LUA_OK && status != LUA_YIELD) {
                const char* error = lua_tostring(thread, -1);
                fprintf(stderr, "[UDC] Error in %s handler: %s\n", event, error ? error : "unknown error");
            }
        }

        lua_pop(L, 1);
    }

    void ControlView::invalidate() {
        layoutDirty = true;
        ++generation;
        surface->invalidate();
    }

    std::shared_ptr<ICanvasImage> ControlView::image(const ImageSource& source) {
        if (source.pixels) {
            auto cached = pixelImages.find(source.pixels.get());
            if (cached != pixelImages.end() && !cached->second.first.expired()) return cached->second.second;

            // Pixels a script replaced are dropped as new ones come
            for (auto it = pixelImages.begin(); it != pixelImages.end();) {
                it = it->second.first.expired() ? pixelImages.erase(it) : std::next(it);
            }
            std::shared_ptr<ICanvasImage> made = surface->createImage(*source.pixels);
            pixelImages[source.pixels.get()] = { source.pixels, made };
            return made;
        }

        if (source.path.empty()) return nullptr;
        auto cached = images.find(source.path);
        if (cached != images.end()) return cached->second;

        Luwow::Engine::Message read;
        read.topic = Luwow::Engine::Topics::PackageRead;
        read.data = source.path;
        std::shared_ptr<ICanvasImage> loaded = (luauHost->request(read) && read.result == 1)
            ? surface->decodeImage(read.data) : surface->loadImage(source.path);
        images[source.path] = loaded;
        return loaded;
    }

    // Cached by the font and the text, until the window's scale changes. The cache is emptied once it's big.
    int ControlView::measureText(const std::string& text, const Font& font) {
        if (text.empty()) return 0;
        Scaler scale = scaler();
        if (scale.scale() != textWidthsScale || textWidths.size() > 8192) {
            textWidths.clear();
            textWidthsScale = scale.scale();
        }

        std::string key = font.family;
        key += '\0';
        key += std::to_string(font.size);
        key += font.bold ? 'b' : '-';
        key += font.italic ? 'i' : '-';
        key += '\0';
        key += text;
        auto cached = textWidths.find(key);
        if (cached != textWidths.end()) return cached->second;

        int width = scale.toLogical(surface->measureText(text, scale.toDevice(font)));
        textWidths.emplace(std::move(key), width);
        return width;
    }

    void ControlView::update() {
        Rect bounds = scaler().toLogical(surface->bounds());
        bool resized = bounds.width != laidOutBounds.width || bounds.height != laidOutBounds.height;
        if (!layoutDirty && !resized) return;

        laidOutBounds = bounds;
        layoutDirty = false;
        rootControl->size = UDim2::fromOffset(static_cast<float>(bounds.width), static_cast<float>(bounds.height));
        layingOut = true;
        ++generation;
        layout(*rootControl);
        layingOut = false;
        ++generation;
    }

    // Parents are arranged before their children, whose fill sizes depend on them
    void ControlView::layout(ControlObject& control) {
        ListLayout* controller = nullptr;
        for (ControlObject* item : control.children) {
            if (!controller && item->kind == ControlKind::ListLayout) controller = static_cast<ListLayout*>(item);

            item->layoutLeft.reset();
            item->layoutTop.reset();
            item->layoutWidth.reset();
            item->layoutHeight.reset();
        }

        if (controller) controller->arrange(control);
        for (ControlObject* child : control.children) {
            if (!child->isHelper()) layout(*child);
        }
    }

    void ControlView::settle() {
        Entry entry(*this);
        update();
    }

    // Controls paint in logical pixels, the scaled canvas turns them into device pixels
    void ControlView::paint(ICanvas& canvas) {
        Entry entry(*this);
        update();
        ScaledCanvas scaled(canvas, surface->scale());
        paintControl(scaled, *rootControl);
    }

    void ControlView::paintControl(ICanvas& canvas, ControlObject& control) {
        if (!control.visible || control.isHelper()) return;

        Rect frame = control.frame();
        bool empty = frame.width <= 0 || frame.height <= 0;

        if (empty) {
            control.paintEmpty(canvas, frame);
        } else {
            control.paint(canvas, frame);
            if (const ControlObject* stroke = control.findHelper(ControlKind::Stroke)) {
                static_cast<const Stroke*>(stroke)->paint(canvas, frame, control.resolveCornerRadii(frame));
            }
        }

        bool clips = control.clips();
        if (!control.children.empty() && !(empty && clips)) {
            if (clips) canvas.pushClip(control.innerFrame());
            for (ControlObject* child : control.drawOrder()) paintControl(canvas, *child);
            if (clips) canvas.popClip();
        }

        if (!empty) control.paintOverlay(canvas, frame);
    }

    ControlObject* ControlView::hitTest(int x, int y, bool activeOnly) {
        update();
        ControlObject* hit = hitTest(*rootControl, x, y, activeOnly);
        return hit == rootControl.get() ? nullptr : hit;
    }

    // Children a control clips are cut off at its edges, so they can't be hit outside it either. The others
    // can stick out of it and be hit there. A control that isn't active lets the point through to the
    // controls behind it, or to its parent.
    ControlObject* ControlView::hitTest(ControlObject& control, int x, int y, bool activeOnly) {
        if (!control.visible || control.isHelper()) return nullptr;
        bool inside = control.frame().contains(x, y);
        bool clips = control.clips();
        if (!inside && clips) return nullptr;
        if (inside && control.capturesPoint(x, y)) return &control;

        // Front to back, the topmost control gets the mouse
        if (!clips || control.innerFrame().contains(x, y)) {
            std::vector<ControlObject*> ordered = control.drawOrder();
            for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
                if (ControlObject* hit = hitTest(**it, x, y, activeOnly)) return hit;
            }
        }
        if (!inside) return nullptr;
        return (!activeOnly || control.active() || &control == rootControl.get()) ? &control : nullptr;
    }

    static std::function<int(lua_State*)> pointerArguments(int x, int y, PointerKind kind) {
        return [x, y, kind](lua_State* L) {
            lua_pushvector(L, static_cast<float>(x), static_cast<float>(y), 0.0f);
            lua_pushstring(L, pointerKindName(kind));
            return 2;
        };
    }

    // An input's table: InputType ("MouseButton1", "MouseButton2", "MouseButton3" or "Touch"), Position in the
    // window like AbsolutePosition, and the Pointer's kind
    static std::function<int(lua_State*)> inputArguments(int x, int y, PointerKind kind, PointerButton button) {
        return [x, y, kind, button](lua_State* L) {
            const char* type = "MouseButton1";
            if (kind == PointerKind::Touch) type = "Touch";
            else if (button == PointerButton::Secondary) type = "MouseButton2";
            else if (button == PointerButton::Middle) type = "MouseButton3";

            lua_createtable(L, 0, 3);
            lua_pushstring(L, type);
            lua_setfield(L, -2, "InputType");
            lua_pushvector(L, static_cast<float>(x), static_cast<float>(y), 0.0f);
            lua_setfield(L, -2, "Position");
            lua_pushstring(L, pointerKindName(kind));
            lua_setfield(L, -2, "Pointer");
            return 1;
        };
    }

    // A press goes to the control under the pointer, which then gets the moves and the release. Presses
    // the bindings list fire OnPressed. A pointer that doesn't hover, such as a touch, enters the control
    // first, and the primary button also moves the focus.
    void ControlView::pointerDown(const PointerInput& input) {
        Entry entry(*this);
        currentModifiers = input.modifiers;
        Scaler scale = scaler();
        int x = pointerX = scale.toLogical(input.x);
        int y = pointerY = scale.toLogical(input.y);
        pointerKind = input.kind;

        bool primary = input.button == PointerButton::Primary;
        bool bound = bindings.presses(input);

        // Every button's press is an input for the control under the pointer, whichever it is
        if (input.button != PointerButton::None) {
            ControlObject* under = hitTest(x, y);
            inputBeganIds[static_cast<int>(input.button)] = under ? under->id : 0;
            if (under) fireEvent(*under, "OnInputBegan", inputArguments(x, y, input.kind, input.button));
        }

        if ((!primary && !bound) || pressedControl) return;

        if (!bindings.hovers(input.kind)) setHovered(hitTest(x, y), x, y, input.kind);

        ControlObject* target = hitTest(x, y);
        uint64_t targetId = target ? target->id : 0;

        // Clicking anything else, or nothing, takes the focus away
        if (primary) setFocus(target && target->focusable() ? target : nullptr);

        // Handlers may have destroyed it
        target = find(targetId);
        if (!target) return;

        pressedControl = target;
        pressedButton = input.button;
        pressedBound = bound;
        target->pressed = true;
        surface->captureMouse(true);
        if (primary) target->onMouseDown(x, y);

        if (bound && find(targetId) == target) fireEvent(*target, "OnPressed", pointerArguments(x, y, input.kind));
    }

    void ControlView::pointerMove(const PointerInput& input) {
        Entry entry(*this);
        Scaler scale = scaler();
        int x = pointerX = scale.toLogical(input.x);
        int y = pointerY = scale.toLogical(input.y);
        pointerKind = input.kind;

        if (pressedControl) pressedControl->onMouseMove(x, y);
        if (bindings.hovers(input.kind)) setHovered(hitTest(x, y), x, y, input.kind);
    }

    // The release goes to the pressed control: OnUnPressed if its press fired OnPressed, then the control's
    // own handling. A pointer that doesn't hover leaves as it lifts.
    void ControlView::pointerUp(const PointerInput& input) {
        Entry entry(*this);
        Scaler scale = scaler();
        int x = pointerX = scale.toLogical(input.x);
        int y = pointerY = scale.toLogical(input.y);
        pointerKind = input.kind;

        // The input ends for the control the press began on, wherever the pointer is by then
        if (input.button != PointerButton::None) {
            uint64_t& began = inputBeganIds[static_cast<int>(input.button)];
            ControlObject* inputControl = began ? find(began) : nullptr;
            began = 0;
            if (inputControl) fireEvent(*inputControl, "OnInputEnded", inputArguments(x, y, input.kind, input.button));
        }

        ControlObject* control = pressedControl;
        if (control && input.button == pressedButton) {
            pressedControl = nullptr;
            pressedButton = PointerButton::None;
            control->pressed = false;
            surface->captureMouse(false);

            uint64_t id = control->id;
            bool inside = control->frame().contains(x, y);
            if (pressedBound) fireEvent(*control, "OnUnPressed", pointerArguments(x, y, input.kind));
            pressedBound = false;

            control = find(id);
            if (control && input.button == PointerButton::Primary) control->onMouseUp(x, y, inside);
        }

        // The release may have changed the tree, so the hover is worked out again
        if (bindings.hovers(input.kind)) setHovered(hitTest(x, y), x, y, input.kind);
        else setHovered(nullptr, x, y, input.kind);
    }

    void ControlView::pointerLeave() {
        Entry entry(*this);
        setHovered(nullptr, pointerX, pointerY, pointerKind);
    }

    // The old control is told first, its OnLeft may run Luau that changes the tree
    void ControlView::setHovered(ControlObject* target, int x, int y, PointerKind kind) {
        if (target == hoveredControl) return;

        ControlObject* old = hoveredControl;
        hoveredControl = target;
        uint64_t targetId = target ? target->id : 0;

        if (old) {
            old->hovered = false;
            old->onHoverChanged();
            fireEvent(*old, "OnLeft", pointerArguments(x, y, kind));
        }

        target = targetId ? find(targetId) : nullptr;
        if (target && hoveredControl == target) {
            target->hovered = true;
            target->onHoverChanged();
            fireEvent(*target, "OnEnter", pointerArguments(x, y, kind));
        }
    }

    // The wheel goes to the control taking input under the mouse, then its parents, until one uses it.
    // When none does, it goes to the controls behind.
    void ControlView::wheel(int x, int y, int dx, int dy) {
        Entry entry(*this);
        Scaler scale = scaler();
        x = scale.toLogical(x);
        y = scale.toLogical(y);

        for (ControlObject* control = hitTest(x, y, true); control; control = control->parent) {
            if (control->onWheel(dx, dy)) return;
        }

        std::vector<ControlObject*> under;
        controlsAt(*rootControl, x, y, under);
        for (ControlObject* control : under) {
            if (control->onWheel(dx, dy)) return;
        }
    }

    void ControlView::controlsAt(ControlObject& control, int x, int y, std::vector<ControlObject*>& found) {
        if (!control.visible || control.isHelper()) return;
        bool inside = control.frame().contains(x, y);
        bool clips = control.clips();
        if (!inside && clips) return;

        if (!clips || control.innerFrame().contains(x, y)) {
            std::vector<ControlObject*> ordered = control.drawOrder();
            for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) controlsAt(**it, x, y, found);
        }
        if (inside && &control != rootControl.get()) found.push_back(&control);
    }

    void ControlView::key(Key key, KeyModifiers modifiers) {
        Entry entry(*this);
        currentModifiers = modifiers;
        if (focusedControl) focusedControl->onKey(key, modifiers);
    }

    void ControlView::text(const std::string& text) {
        Entry entry(*this);
        if (focusedControl) focusedControl->onText(text);
    }

    void ControlView::setWindowFocused(bool focused) {
        Entry entry(*this);
        windowFocused = focused;
        surface->invalidate();
    }
} // namespace Luwow::Udc
