#pragma once

#include <string>
#include <functional>
#include "lua.h"

namespace Luwow::Engine {
class ILuauHost;
}

namespace Luwow::Udc {

struct WindowDescriptor {
    std::string Name = "";
    std::string Title = "";
    int Width = 0;
    int Height = 0;
    int Left = 0;
    int Top = 0;
    // Whether the user can resize (and maximize) the window
    bool Resizable = true;
    // An .ico or image file (PNG, JPEG, BMP or GIF) shown in the title bar and the taskbar, empty for the default
    std::string Icon = "";
    // Whether it stays above the windows of other programs
    bool TopMost = false;
    // Whether it can be minimized, true when left out
    bool Minimizable = true;
    // Opens in the middle of its owner, or of the screen, instead of at Position
    bool Centered = false;
    // When set, the size of the area inside the frame, which Size is then worked out from
    int ClientWidth = 0;
    int ClientHeight = 0;
};

// Returns the parent's stack index, pushing the field's value when it's the one used. Raises an error
// when there's no parent, or when both are given and differ.
int getParentIndex(lua_State* L);

// Methods for getting the descriptors from their respective Luau tables.
WindowDescriptor getWindowDescriptor(lua_State* L);

} // namespace Luwow::Udc