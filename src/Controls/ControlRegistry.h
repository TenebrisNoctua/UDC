#pragma once

#include "ControlView.h"

namespace Luwow::Udc {
    // Registers every control class, and makes the udc.create*
    // functions. Each class's own members and descriptor fields live in its own files.

    // Registers every control class, and the helper types their members use
    void registerControlClasses(lua_State* L, Luwow::Engine::ILuauHost* host);

    // The control at index, nullptr for anything else, including destroyed controls
    ControlObject* toControl(lua_State* L, int index);

    // The control to put a child of the parent at index in.
    ControlObject* checkParent(lua_State* L, int index);

    // udc.createView(descriptor, parent)
    int createView(lua_State* L);
    int createScrollView(lua_State* L);
    int createTextLabel(lua_State* L);
    int createTextButton(lua_State* L);
    int createTextInput(lua_State* L);
    int createListLayout(lua_State* L);
    int createCorner(lua_State* L);
    int createImageLabel(lua_State* L);
    int createPadding(lua_State* L);
    int createStroke(lua_State* L);
    int createImageButton(lua_State* L);
    int createGradient(lua_State* L);
} // namespace Luwow::Udc
