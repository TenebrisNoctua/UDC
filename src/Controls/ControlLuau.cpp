#include "ControlLuau.h"
#include "ControlObject.h"

#include "ColorCore.h"

namespace Luwow::Udc::ControlLuau {
    using Luwow::Colors::Color;

    static bool isNone(lua_State* L, int index) {
        return lua_isnoneornil(L, index) || (lua_isboolean(L, index) && !lua_toboolean(L, index));
    }

    std::optional<uint32_t> checkColor(lua_State* L, int index) {
        if (isNone(L, index)) return std::nullopt;
        Color color = Luwow::Colors::checkColor(L, index);
        return makeColor(color.toHex(), color.alphaByte());
    }

    void pushColor(lua_State* L, std::optional<uint32_t> color) {
        if (!color) {
            lua_pushnil(L);
            return;
        }
        Color value = Color::fromHex(colorRgb(*color));
        value.a = static_cast<float>(colorAlpha(*color)) / 255.0f;
        Luwow::Colors::pushColor(L, value);
    }

    void pushVector2(lua_State* L, float x, float y) {
        lua_pushvector(L, x, y, 0.0f);
    }

    int checkSize(lua_State* L, int index) {
        int size = luaL_checkinteger(L, index);
        if (size < 0) luaL_argerror(L, index, "must be non-negative");
        return size;
    }

    void checkSizeVector(lua_State* L, int index, int& width, int& height) {
        const float* v = luaL_checkvector(L, index);
        width = static_cast<int>(v[0]);
        height = static_cast<int>(v[1]);
        if (width < 0 || height < 0) luaL_argerror(L, index, "components must be non-negative");
    }

    std::optional<UDim2> checkOptionalUDim2(lua_State* L, int index) {
        if (isNone(L, index)) return std::nullopt;
        return Luwow::UDims::checkUDim2Like(L, index);
    }

    void pushOptionalUDim2(lua_State* L, const std::optional<UDim2>& value) {
        if (value) Luwow::UDims::pushUDim2(L, *value);
        else lua_pushnil(L);
    }

    bool field(lua_State* L, const char* name) {
        lua_getfield(L, 1, name);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    void readInt(lua_State* L, const char* name, int& out) {
        if (!field(L, name)) return;
        out = static_cast<int>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
    }

    void readBool(lua_State* L, const char* name, bool& out) {
        if (!field(L, name)) return;
        out = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    void readString(lua_State* L, const char* name, std::string& out) {
        if (!field(L, name)) return;
        out = luaL_checkstring(L, -1);
        lua_pop(L, 1);
    }

    void readUDim2(lua_State* L, const char* name, UDim2& out) {
        if (!field(L, name)) return;
        out = Luwow::UDims::checkUDim2Like(L, -1);
        lua_pop(L, 1);
    }

    void readOptionalUDim2(lua_State* L, const char* name, std::optional<UDim2>& out) {
        if (!field(L, name)) return;
        out = checkOptionalUDim2(L, -1);
        lua_pop(L, 1);
    }

    void readColor(lua_State* L, const char* name, std::optional<uint32_t>& out) {
        if (!field(L, name)) return;
        out = checkColor(L, -1);
        lua_pop(L, 1);
    }

    void readColor(lua_State* L, const char* name, uint32_t& out) {
        std::optional<uint32_t> color = out;
        readColor(L, name, color);
        if (color) out = *color;
    }

    void readHandler(lua_State* L, const char* name, ControlObject& control) {
        if (!field(L, name)) return;
        luaL_checktype(L, -1, LUA_TFUNCTION);
        control.handlers[name] = lua_ref(L, -1);
        lua_pop(L, 1);
    }
}
