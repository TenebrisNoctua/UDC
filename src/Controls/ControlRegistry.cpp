#include "ControlRegistry.h"
#include "UdcModule.h"
#include "Handles.h"

#include "Corner.h"
#include "Gradient.h"
#include "ImageButton.h"
#include "ImageLabel.h"
#include "ListLayout.h"
#include "Padding.h"
#include "ScrollView.h"
#include "Stroke.h"
#include "TextButton.h"
#include "TextInput.h"
#include "TextLabel.h"
#include "View.h"

#include "ColorCore.h"
#include "RectCore.h"
#include "UDimCore.h"
#include "SignalCore.h"

#include "lua.h"
#include "lualib.h"

namespace Luwow::Udc {
    void registerControlClasses(lua_State* L, Luwow::Engine::ILuauHost* host) {
        Luwow::Colors::registerColor(L);
        Luwow::Rects::registerRect(L);
        Luwow::UDims::registerUDims(L);
        Luwow::Signals::registerSignals(L, host);

        View::RegisterClass(L);
        ScrollView::RegisterClass(L);
        TextLabel::RegisterClass(L);
        TextButton::RegisterClass(L);
        TextInput::RegisterClass(L);
        ListLayout::RegisterClass(L);
        Corner::RegisterClass(L);
        ImageLabel::RegisterClass(L);
        Padding::RegisterClass(L);
        Stroke::RegisterClass(L);
        ImageButton::RegisterClass(L);
        Gradient::RegisterClass(L);
    }

    ControlObject* toControl(lua_State* L, int index) {
        return dynamic_cast<ControlObject*>(Luwow::Objects::BaseObject::to(L, index));
    }

    ControlObject* checkParent(lua_State* L, int index) {
        // Another library's window, such as the gui library's, is drawn into by a UDC window of its own
        if (LuauWindow* attached = attachedWindow(L, index, false)) return &attached->controls()->root();
        if (!Luwow::Objects::BaseObject::is(L, index)) luaL_typeerror(L, index, "Window or ControlObject");

        // Raises for destroyed objects
        Luwow::Objects::BaseObject* object = Luwow::Objects::BaseObject::check(L, index);
        if (auto* window = dynamic_cast<LuauWindow*>(object)) return &window->controls()->root();

        auto* parent = dynamic_cast<ControlObject*>(object);
        if (!parent || parent->isHelper()) luaL_typeerror(L, index, "Window or ControlObject");
        return parent;
    }

    // Drawn controls can be created from any thread holding the state lock.
    template <typename T>
    static int createControl(lua_State* L) {
        // The parent is checked first, so a bad one doesn't leak the descriptor's handler references. It's
        // the second argument, or the descriptor's Parent.
        bool hasDescriptor = !lua_isnoneornil(L, 1);
        if (hasDescriptor) luaL_checktype(L, 1, LUA_TTABLE);
        ControlObject* parent = checkParent(L, getParentIndex(L));

        // The control lives in its userdata, left on the stack as the result
        T* control = T::Push(L);
        if (hasDescriptor) control->readDescriptor(L);
        parent->view->add(control, parent, L);
        return 1;
    }

    int createView(lua_State* L) { return createControl<View>(L); }
    int createScrollView(lua_State* L) { return createControl<ScrollView>(L); }
    int createTextLabel(lua_State* L) { return createControl<TextLabel>(L); }
    int createTextButton(lua_State* L) { return createControl<TextButton>(L); }
    int createTextInput(lua_State* L) { return createControl<TextInput>(L); }
    int createListLayout(lua_State* L) { return createControl<ListLayout>(L); }
    int createCorner(lua_State* L) { return createControl<Corner>(L); }
    int createImageLabel(lua_State* L) { return createControl<ImageLabel>(L); }
    int createPadding(lua_State* L) { return createControl<Padding>(L); }
    int createStroke(lua_State* L) { return createControl<Stroke>(L); }
    int createImageButton(lua_State* L) { return createControl<ImageButton>(L); }
    int createGradient(lua_State* L) { return createControl<Gradient>(L); }
} // namespace Luwow::Udc
