#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

#include <vector>

namespace Luwow::Udc {
    // Colors its parent's background along a line.
    class Gradient : public ControlObject, public Luwow::Objects::ObjectUserdata<Gradient> {
    public:
        static constexpr const char* Name = "Gradient";
        // Scripts don't see it as a ControlObject so it isn't drawn
        using Super = BaseGuiObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        Gradient() : ControlObject(ControlKind::Gradient) {}

        // A value at a point along the gradient, from 0 at its start to 1 at its end, in order
        struct ColorKeypoint {
            float time = 0;
            // Its alpha multiplies the opacity too
            uint32_t color = 0xFFFFFF;
        };
        struct NumberKeypoint {
            float time = 0;
            float value = 0;
        };

        std::vector<ColorKeypoint> colors{ { 0, 0xFFFFFF }, { 1, 0xFFFFFF } };
        std::vector<NumberKeypoint> transparency{ { 0, 0 }, { 1, 0 } };
        // Degrees, clockwise from left to right
        float rotation = 0;
        // Moves the gradient by a fraction of the parent's size
        float offsetX = 0;
        float offsetY = 0;
        bool enabled = true;

        // The stops a background of the given color is painted with
        std::vector<GradientStop> stops(uint32_t background) const;

        bool isHelper() const override { return true; }

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
