#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    enum class FillDirection { Vertical, Horizontal };
    enum class HorizontalAlignment { Left, Center, Right };
    enum class VerticalAlignment { Top, Center, Bottom };
    enum class SortOrder { LayoutOrder, Name };
    enum class FlexAlignment { None, Fill, SpaceAround, SpaceBetween, SpaceEvenly };

    // Arranges its parent's other children in a row or column
    class ListLayout : public ControlObject, public Luwow::Objects::ObjectUserdata<ListLayout> {
    public:
        static constexpr const char* Name = "ListLayout";
        // Scripts don't see it as a ControlObject since it isn't drawn
        using Super = BaseGuiObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        ListLayout() : ControlObject(ControlKind::ListLayout) {}

        bool isHelper() const override { return true; }

        FillDirection fillDirection = FillDirection::Vertical;
        HorizontalAlignment horizontalAlignment = HorizontalAlignment::Left;
        VerticalAlignment verticalAlignment = VerticalAlignment::Top;
        SortOrder sortOrder = SortOrder::LayoutOrder;
        UDim padding;
        bool wraps = false;
        FlexAlignment horizontalFlex = FlexAlignment::None;
        FlexAlignment verticalFlex = FlexAlignment::None;
        ItemLineAlignment itemLineAlignment = ItemLineAlignment::Automatic;

        // The size of the arranged content after the last layout
        int contentWidth = 0;
        int contentHeight = 0;

        // Positions and sizes the container's drawn children
        void arrange(ControlObject& container);

        // The size the container's drawn children take when arranged in a content area of width by height, on one line
        void measure(const ControlObject& container, float width, float height, int& contentWidth, int& contentHeight) const;

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
