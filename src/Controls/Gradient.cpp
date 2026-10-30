#include "Gradient.h"

#include <algorithm>
#include <cmath>

namespace Luwow::Udc {
    static constexpr float Pi = 3.14159265358979f;

    // Each byte, the transparency one too, blended on its own
    static uint32_t lerpColor(uint32_t from, uint32_t to, float alpha) {
        auto channel = [alpha](uint32_t a, uint32_t b, int shift) {
            float start = static_cast<float>((a >> shift) & 0xFF);
            float end = static_cast<float>((b >> shift) & 0xFF);
            return static_cast<uint32_t>(std::lround(start + (end - start) * alpha)) << shift;
        };
        return channel(from, to, 24) | channel(from, to, 16) | channel(from, to, 8) | channel(from, to, 0);
    }

    // The value between the keypoints around time, the end ones past them
    template <typename Keypoint, typename Value, typename Lerp>
    static Value valueAt(const std::vector<Keypoint>& keypoints, float time, Value Keypoint::*field, Lerp lerp) {
        if (time <= keypoints.front().time) return keypoints.front().*field;
        for (size_t index = 1; index < keypoints.size(); ++index) {
            const Keypoint& next = keypoints[index];
            if (time > next.time) continue;
            const Keypoint& previous = keypoints[index - 1];
            float span = next.time - previous.time;
            float alpha = span > 0 ? (time - previous.time) / span : 1.0f;
            return lerp(previous.*field, next.*field, alpha);
        }
        return keypoints.back().*field;
    }

    // A stop wherever a keypoint is, once the offset has moved them, and at both ends. The background's
    // channels are multiplied by the gradient's, its opacity by the color's and the transparency's.
    std::vector<GradientStop> Gradient::stops(uint32_t background) const {
        // The offset moves the gradient along its line by the part of it in the line's direction
        float radians = rotation * Pi / 180.0f;
        float shift = offsetX * std::cos(radians) + offsetY * std::sin(radians);

        std::vector<float> times{ 0, 1 };
        for (const ColorKeypoint& keypoint : colors) times.push_back(keypoint.time + shift);
        for (const NumberKeypoint& keypoint : transparency) times.push_back(keypoint.time + shift);
        times.erase(std::remove_if(times.begin(), times.end(), [](float time) { return time < 0 || time > 1; }), times.end());
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end(), [](float a, float b) { return std::abs(a - b) < 1e-4f; }), times.end());

        auto lerpNumber = [](float a, float b, float alpha) { return a + (b - a) * alpha; };
        std::vector<GradientStop> result;
        for (float time : times) {
            float along = std::clamp(time - shift, 0.0f, 1.0f);
            uint32_t color = valueAt(colors, along, &ColorKeypoint::color, lerpColor);
            float clear = std::clamp(valueAt(transparency, along, &NumberKeypoint::value, lerpNumber), 0.0f, 1.0f);

            auto multiply = [&](int shift) {
                return (((background >> shift) & 0xFF) * ((color >> shift) & 0xFF) / 255) << shift;
            };
            uint32_t rgb = multiply(16) | multiply(8) | multiply(0);
            float opacity = colorAlpha(background) / 255.0f * colorAlpha(color) / 255.0f * (1 - clear);
            result.push_back({ time, makeColor(rgb, static_cast<uint32_t>(std::lround(opacity * 255))) });
        }
        return result;
    }

    // Luau

    static float checkTime(lua_State* L, int index) {
        float time = static_cast<float>(luaL_checknumber(L, index));
        if (time < 0 || time > 1) luaL_error(L, "a keypoint's Time must be from 0 to 1");
        return time;
    }

    // Each entry of the table at index is a keypoint ({Time = number, Value = ...}), or a value placed evenly
    // along the gradient. read turns the value at the top of the stack into the keypoint's.
    template <typename Keypoint, typename Read>
    static std::vector<Keypoint> checkKeypoints(lua_State* L, int index, Read read) {
        index = lua_absindex(L, index);
        luaL_checktype(L, index, LUA_TTABLE);
        int count = lua_objlen(L, index);
        if (count == 0) luaL_argerror(L, index, "at least one keypoint expected");

        std::vector<Keypoint> keypoints;
        for (int entry = 1; entry <= count; ++entry) {
            lua_rawgeti(L, index, entry);
            Keypoint keypoint;
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "Time");
                keypoint.time = checkTime(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, -1, "Value");
                read(L, keypoint);
                lua_pop(L, 1);
            } else {
                keypoint.time = count == 1 ? 0.0f : static_cast<float>(entry - 1) / (count - 1);
                read(L, keypoint);
            }
            lua_pop(L, 1);
            keypoints.push_back(keypoint);
        }

        std::stable_sort(keypoints.begin(), keypoints.end(), [](const Keypoint& a, const Keypoint& b) { return a.time < b.time; });
        return keypoints;
    }

    // A Color, a list of them spread evenly, or {Time, Value} keypoints
    static std::vector<Gradient::ColorKeypoint> checkColors(lua_State* L, int index) {
        auto read = [](lua_State* L, Gradient::ColorKeypoint& keypoint) {
            std::optional<uint32_t> color = ControlLuau::checkColor(L, -1);
            if (!color) luaL_error(L, "a gradient's colors must be Colors");
            keypoint.color = *color;
        };
        if (!lua_istable(L, index)) {
            Gradient::ColorKeypoint keypoint;
            read(L, keypoint);
            return { { 0, keypoint.color }, { 1, keypoint.color } };
        }
        return checkKeypoints<Gradient::ColorKeypoint>(L, index, read);
    }

    // A number, a list of them spread evenly, or {Time, Value} keypoints
    static std::vector<Gradient::NumberKeypoint> checkTransparency(lua_State* L, int index) {
        auto read = [](lua_State* L, Gradient::NumberKeypoint& keypoint) {
            keypoint.value = static_cast<float>(luaL_checknumber(L, -1));
        };
        if (!lua_istable(L, index)) {
            float value = static_cast<float>(luaL_checknumber(L, index));
            return { { 0, value }, { 1, value } };
        }
        return checkKeypoints<Gradient::NumberKeypoint>(L, index, read);
    }

    // Read back as keypoints, {Time = number, Value = ...}
    template <typename Keypoint, typename Push>
    static void pushKeypoints(lua_State* L, const std::vector<Keypoint>& keypoints, Push push) {
        lua_createtable(L, static_cast<int>(keypoints.size()), 0);
        for (size_t index = 0; index < keypoints.size(); ++index) {
            lua_createtable(L, 0, 2);
            lua_pushnumber(L, keypoints[index].time);
            lua_setfield(L, -2, "Time");
            push(L, keypoints[index]);
            lua_setfield(L, -2, "Value");
            lua_rawseti(L, -2, static_cast<int>(index + 1));
        }
    }

    void Gradient::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        ControlObject::readDescriptor(L);
        if (field(L, "Color")) {
            colors = checkColors(L, -1);
            lua_pop(L, 1);
        }
        if (field(L, "Transparency")) {
            transparency = checkTransparency(L, -1);
            lua_pop(L, 1);
        }
        if (field(L, "Rotation")) {
            rotation = static_cast<float>(luaL_checknumber(L, -1));
            lua_pop(L, 1);
        }
        if (field(L, "Offset")) {
            const float* offset = luaL_checkvector(L, -1);
            offsetX = offset[0];
            offsetY = offset[1];
            lua_pop(L, 1);
        }
        readBool(L, "Enabled", enabled);
    }

    static int getColor(lua_State* L, Gradient& object) {
        pushKeypoints(L, object.colors, [](lua_State* L, const Gradient::ColorKeypoint& keypoint) {
            ControlLuau::pushColor(L, keypoint.color);
        });
        return 1;
    }

    static int setColor(lua_State* L, Gradient& object) {
        object.colors = checkColors(L, 3);
        object.invalidate();
        return 0;
    }

    static int getTransparency(lua_State* L, Gradient& object) {
        pushKeypoints(L, object.transparency, [](lua_State* L, const Gradient::NumberKeypoint& keypoint) {
            lua_pushnumber(L, keypoint.value);
        });
        return 1;
    }

    static int setTransparency(lua_State* L, Gradient& object) {
        object.transparency = checkTransparency(L, 3);
        object.invalidate();
        return 0;
    }

    static int getRotation(lua_State* L, Gradient& object) {
        lua_pushnumber(L, object.rotation);
        return 1;
    }

    static int setRotation(lua_State* L, Gradient& object) {
        object.rotation = static_cast<float>(luaL_checknumber(L, 3));
        object.invalidate();
        return 0;
    }

    static int getOffset(lua_State* L, Gradient& object) {
        ControlLuau::pushVector2(L, object.offsetX, object.offsetY);
        return 1;
    }

    static int setOffset(lua_State* L, Gradient& object) {
        const float* offset = luaL_checkvector(L, 3);
        object.offsetX = offset[0];
        object.offsetY = offset[1];
        object.invalidate();
        return 0;
    }

    static int getEnabled(lua_State* L, Gradient& object) {
        lua_pushboolean(L, object.enabled);
        return 1;
    }

    static int setEnabled(lua_State* L, Gradient& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.enabled = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }

    void Gradient::RegisterClass(lua_State* L) {
        RegisterGuiObject<Gradient>(L, {}, {
            { "Color", Bind<getColor>, Bind<setColor> },
            { "Transparency", Bind<getTransparency>, Bind<setTransparency> },
            { "Rotation", Bind<getRotation>, Bind<setRotation> },
            { "Offset", Bind<getOffset>, Bind<setOffset> },
            { "Enabled", Bind<getEnabled>, Bind<setEnabled> },
        });
    }
} // namespace Luwow::Udc
