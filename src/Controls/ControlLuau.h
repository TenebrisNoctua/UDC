#pragma once

#include "UDimCore.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "lua.h"
#include "lualib.h"

// Utility for Luau members and descriptors.
namespace Luwow::Udc {
    class ControlObject;
}

namespace Luwow::Udc::ControlLuau {
    using UDim2 = Luwow::UDims::UDim2;

    // Values

    std::optional<uint32_t> checkColor(lua_State* L, int index);
    void pushColor(lua_State* L, std::optional<uint32_t> color);

    void pushVector2(lua_State* L, float x, float y);

    // A non-negative integer
    int checkSize(lua_State* L, int index);
    // A vector whose components can't be negative, in whole pixels
    void checkSizeVector(lua_State* L, int index, int& width, int& height);

    // A UDim2 or a vector of pixels, nil or false meaning none
    std::optional<UDim2> checkOptionalUDim2(lua_State* L, int index);
    void pushOptionalUDim2(lua_State* L, const std::optional<UDim2>& value);

    // Enums are passed as their names, names lists them in the enum's order and ends with nullptr
    template <typename E>
    E checkEnum(lua_State* L, int index, const char* const names[]) {
        return static_cast<E>(luaL_checkoption(L, index, nullptr, names));
    }

    template <typename E>
    void pushEnum(lua_State* L, E value, const char* const names[]) {
        lua_pushstring(L, names[static_cast<int>(value)]);
    }

    // Descriptor fields, read from the descriptor table at index 1. A nil field leaves the value unchanged.

    // Pushes descriptor[name] and returns true, or pushes nothing and returns false when it's nil
    bool field(lua_State* L, const char* name);

    void readInt(lua_State* L, const char* name, int& out);
    void readBool(lua_State* L, const char* name, bool& out);
    void readString(lua_State* L, const char* name, std::string& out);
    void readUDim2(lua_State* L, const char* name, UDim2& out);
    void readOptionalUDim2(lua_State* L, const char* name, std::optional<UDim2>& out);
    // A color field set to false clears the control's default
    void readColor(lua_State* L, const char* name, std::optional<uint32_t>& out);
    void readColor(lua_State* L, const char* name, uint32_t& out);
    // A function, kept as a handler run for the event of the same name
    void readHandler(lua_State* L, const char* name, ControlObject& control);

    template <typename E>
    void readEnum(lua_State* L, const char* name, E& out, const char* const names[]) {
        if (!field(L, name)) return;
        out = checkEnum<E>(L, -1, names);
        lua_pop(L, 1);
    }

    // Member tables

    template <typename T>
    using Properties = std::vector<typename T::PropertyDef>;

    // The members of the classes a class builds on, followed by the class's own
    template <typename T>
    Properties<T> extend(Properties<T> inherited, std::initializer_list<typename T::PropertyDef> own) {
        inherited.insert(inherited.end(), own);
        return inherited;
    }
}
