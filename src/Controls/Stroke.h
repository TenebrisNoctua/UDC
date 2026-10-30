#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    // Where a stroke's line goes along the parent's edge
    enum class StrokePosition { Outer, Center, Inner };
    extern const char* const StrokePositionNames[];

    // Outlines its parent, a line of Thickness pixels along the parent's frame,
    // outside it by default, following its rounded corners.
    class Stroke : public ControlObject, public Luwow::Objects::ObjectUserdata<Stroke> {
    public:
        static constexpr const char* Name = "Stroke";
        // Scripts don't see it as a ControlObject since it isn't drawn
        using Super = BaseGuiObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        Stroke() : ControlObject(ControlKind::Stroke) {}

        uint32_t color = 0x000000;
        int thickness = 1;
        StrokePosition position = StrokePosition::Outer;

        // Paints the outline around the parent's frame, whose corners have the given radii
        void paint(ICanvas& canvas, const Rect& parentFrame, const CornerRadii& parentRadii) const;

        bool isHelper() const override { return true; }

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
