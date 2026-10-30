#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    // Rounds its parent's corners.
    class Corner : public ControlObject, public Luwow::Objects::ObjectUserdata<Corner> {
    public:
        static constexpr const char* Name = "Corner";
        // Scripts don't see it as a ControlObject since it isn't drawn
        using Super = BaseGuiObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        Corner() : ControlObject(ControlKind::Corner) {}

        // Each scale is a fraction of the parent's shorter side
        UDim topLeft = UDim{ 0, 8 };
        UDim topRight = UDim{ 0, 8 };
        UDim bottomRight = UDim{ 0, 8 };
        UDim bottomLeft = UDim{ 0, 8 };

        // The radii in pixels for a frame
        CornerRadii resolve(const Rect& frame) const;

        bool isHelper() const override { return true; }

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
