#include "Stroke.h"

namespace Luwow::Udc {
    // The canvas strokes inside the rectangle it's given, so it's given the frame grown by the thickness
    const char* const StrokePositionNames[] = { "Outer", "Center", "Inner", nullptr };

    // The line is drawn inside the outline, so it's the frame grown by how far the line reaches out of it
    void Stroke::paint(ICanvas& canvas, const Rect& parentFrame, const CornerRadii& parentRadii) const {
        if (thickness <= 0) return;
        int outside = position == StrokePosition::Outer ? thickness : position == StrokePosition::Center ? thickness / 2 : 0;
        Rect outline{ parentFrame.x - outside, parentFrame.y - outside, parentFrame.width + outside * 2, parentFrame.height + outside * 2 };
        canvas.strokeRect(outline, color, thickness, parentRadii.grown(outside));
    }

    // Luau

    void Stroke::readDescriptor(lua_State* L) {
        ControlObject::readDescriptor(L);
        ControlLuau::readColor(L, "Color", color);
        if (ControlLuau::field(L, "Thickness")) {
            thickness = ControlLuau::checkSize(L, -1);
            lua_pop(L, 1);
        }
        ControlLuau::readEnum(L, "StrokePosition", position, StrokePositionNames);
    }

    static int getColor(lua_State* L, Stroke& object) {
        ControlLuau::pushColor(L, object.color);
        return 1;
    }

    static int setColor(lua_State* L, Stroke& object) {
        std::optional<uint32_t> color = ControlLuau::checkColor(L, 3);
        if (!color) luaL_argerror(L, 3, "Color expected");
        object.color = *color;
        object.invalidate();
        return 0;
    }

    // Whole pixels, 0 draws nothing
    static int getThickness(lua_State* L, Stroke& object) {
        lua_pushinteger(L, object.thickness);
        return 1;
    }

    static int setThickness(lua_State* L, Stroke& object) {
        object.thickness = ControlLuau::checkSize(L, 3);
        object.invalidate();
        return 0;
    }

    static int getStrokePosition(lua_State* L, Stroke& object) {
        ControlLuau::pushEnum(L, object.position, StrokePositionNames);
        return 1;
    }

    static int setStrokePosition(lua_State* L, Stroke& object) {
        object.position = ControlLuau::checkEnum<StrokePosition>(L, 3, StrokePositionNames);
        object.invalidate();
        return 0;
    }

    void Stroke::RegisterClass(lua_State* L) {
        RegisterGuiObject<Stroke>(L, {}, {
            { "Color", Bind<getColor>, Bind<setColor> },
            { "Thickness", Bind<getThickness>, Bind<setThickness> },
            { "StrokePosition", Bind<getStrokePosition>, Bind<setStrokePosition> },
        });
    }
} // namespace Luwow::Udc
