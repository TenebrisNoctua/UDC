#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    // A container whose children can be larger than it, scrolled with the mouse wheel or its scroll bars
    class ScrollView : public ControlObject, public Luwow::Objects::ObjectUserdata<ScrollView> {
    public:
        static constexpr const char* Name = "ScrollView";
        using Super = ControlObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        ScrollView() : ControlObject(ControlKind::ScrollView) {}

        int scrollX = 0;
        int scrollY = 0;
        // The scrollable size, 0 measures the children
        int contentWidth = 0;
        int contentHeight = 0;
        // The axes the scrollable size grows along to fit the children
        AutomaticSize automaticCanvasSize = AutomaticSize::None;
        uint32_t scrollBarColor = 0xA0A0A0;
        int scrollBarThickness = 10;

        // The content size used for scrolling
        int contentSizeX() const;
        int contentSizeY() const;
        // Keeps the scroll position within the content
        void clampScroll();

        void childOriginIn(const Rect& content, int& x, int& y) const override;
        void paint(ICanvas& canvas, const Rect& frame) override;
        void paintOverlay(ICanvas& canvas, const Rect& frame) override;
        bool capturesPoint(int x, int y) const override;
        bool activeByDefault() const override { return true; }
        // Its content scrolls within it, so it's always cut off at the edges
        bool clips() const override { return true; }

        void onMouseDown(int x, int y) override;
        void onMouseMove(int x, int y) override;
        void onMouseUp(int x, int y, bool inside) override;
        bool onWheel(int dx, int dy) override;

        // Luau
        void readDescriptor(lua_State* L) override;

    private:
        struct Bar {
            bool visible = false;
            Rect track;
            Rect thumb;
        };

        // axis 0 is horizontal, 1 is vertical
        Bar bar(int axis) const;

        int dragAxis = -1;
        int dragStart = 0;
        int dragScrollStart = 0;
    };
} // namespace Luwow::Udc
