#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    // Insets its parent's content
    class Padding : public ControlObject, public Luwow::Objects::ObjectUserdata<Padding> {
    public:
        static constexpr const char* Name = "Padding";
        // Scripts don't see it as a ControlObject since it isn't drawn
        using Super = BaseGuiObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        Padding() : ControlObject(ControlKind::Padding) {}

        // Each side's scale is a fraction of the parent's size along it, the width for left and right,
        // the height for top and bottom
        UDim top;
        UDim bottom;
        UDim left;
        UDim right;

        // The area within the insets, never smaller than nothing
        Rect inset(const Rect& area) const;
        // The sides' total in pixels for an area of that size, even where they don't fit in it
        void insets(float width, float height, int& horizontal, int& vertical) const;

        bool isHelper() const override { return true; }

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
