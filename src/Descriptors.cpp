#include "Descriptors.h"
#include <cmath>
#include "lua.h"
#include "lualib.h"

namespace Luwow::Udc {

int getParentIndex(lua_State* L) {
    bool hasArgument = !lua_isnoneornil(L, 2);
    if (lua_istable(L, 1)) lua_getfield(L, 1, "Parent");
    else lua_pushnil(L);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        if (!hasArgument) luaL_error(L, "a parent is needed, as the descriptor's Parent or the second argument");
        return 2;
    }

    if (hasArgument) {
        if (!lua_rawequal(L, -1, 2)) luaL_error(L, "the descriptor's Parent and the second argument are different objects");
        lua_pop(L, 1);
        return 2;
    }
    return lua_gettop(L);
}

// Reads a vector field into whole pixels, leaving x and y unchanged when it's nil
static void readVector(lua_State* L, const char* name, int& x, int& y) {
    lua_getfield(L, 1, name);
    if (!lua_isnil(L, -1)) {
        const float* v = luaL_checkvector(L, -1);
        x = static_cast<int>(std::lround(v[0]));
        y = static_cast<int>(std::lround(v[1]));
    }
    lua_pop(L, 1);
}

// Reads a boolean field, leaving value unchanged when it's nil
static void readBool(lua_State* L, const char* name, bool& value) {
    lua_getfield(L, 1, name);
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        value = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
}

// Reads the descriptor's Position and Size vectors, which default to 0
static void readBounds(lua_State* L, int& left, int& top, int& width, int& height) {
    readVector(L, "Position", left, top);
    readVector(L, "Size", width, height);
}

// Helper function to get the WindowDescriptor from the table.
WindowDescriptor getWindowDescriptor(lua_State* L) {
    WindowDescriptor windowDescriptor;
    luaL_checktype(L, 1, LUA_TTABLE);
    
    
    lua_getfield(L, 1, "Title");
    windowDescriptor.Title = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "Name");
    if (!lua_isnil(L, -1)) windowDescriptor.Name = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    readBounds(L, windowDescriptor.Left, windowDescriptor.Top, windowDescriptor.Width, windowDescriptor.Height);

    lua_getfield(L, 1, "Icon");
    if (!lua_isnil(L, -1)) windowDescriptor.Icon = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    // Left out, the window is resizable
    readBool(L, "Resizable", windowDescriptor.Resizable);
    readBool(L, "TopMost", windowDescriptor.TopMost);
    readBool(L, "Minimizable", windowDescriptor.Minimizable);
    readBool(L, "Centered", windowDescriptor.Centered);
    readVector(L, "ClientSize", windowDescriptor.ClientWidth, windowDescriptor.ClientHeight);

    return windowDescriptor;
}

} // namespace Luwow::Udc
