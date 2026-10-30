#include "ScrollView.h"
#include "ControlView.h"

#include <algorithm>

namespace Luwow::Udc {
    static constexpr int MinimumThumb = 20;

    // The children's furthest edges, ignoring the scroll position
    static void measureChildren(const ScrollView& view, int& widthOut, int& heightOut) {
        widthOut = 0;
        heightOut = 0;
        for (const ControlObject* child : view.children) {
            if (!child->visible || child->isHelper()) continue;
            Rect local = child->localFrame();
            widthOut = std::max(widthOut, local.right());
            heightOut = std::max(heightOut, local.bottom());
        }
    }

    // An automatic axis is the larger of ContentSize and the children, otherwise ContentSize, or the
    // children when it's 0
    int ScrollView::contentSizeX() const {
        if (contentWidth > 0 && !growsX(automaticCanvasSize)) return contentWidth;
        int w, h;
        measureChildren(*this, w, h);
        return std::max(contentWidth, w);
    }

    int ScrollView::contentSizeY() const {
        if (contentHeight > 0 && !growsY(automaticCanvasSize)) return contentHeight;
        int w, h;
        measureChildren(*this, w, h);
        return std::max(contentHeight, h);
    }

    // The children's frames move with the scroll position, so the cached ones go when it changes
    void ScrollView::clampScroll() {
        Rect viewport = innerFrame();
        int oldX = scrollX;
        int oldY = scrollY;
        scrollX = std::clamp(scrollX, 0, std::max(0, contentSizeX() - viewport.width));
        scrollY = std::clamp(scrollY, 0, std::max(0, contentSizeY() - viewport.height));
        if ((scrollX != oldX || scrollY != oldY) && view) view->invalidateFrames();
    }

    void ScrollView::childOriginIn(const Rect& content, int& x, int& y) const {
        x = content.x - scrollX;
        y = content.y - scrollY;
    }

    // The bars sit inside the border
    ScrollView::Bar ScrollView::bar(int axis) const {
        Rect viewport = innerFrame();
        int content = axis == 0 ? contentSizeX() : contentSizeY();
        int visibleSize = axis == 0 ? viewport.width : viewport.height;
        int thickness = std::max(0, scrollBarThickness);

        Bar result;
        result.visible = thickness > 0 && content > visibleSize && visibleSize > thickness * 2;
        if (!result.visible) return result;

        // Leaves the corner free when both bars show
        int otherContent = axis == 0 ? contentSizeY() : contentSizeX();
        int otherSize = axis == 0 ? viewport.height : viewport.width;
        int corner = otherContent > otherSize ? thickness : 0;

        int scroll = axis == 0 ? scrollX : scrollY;
        if (axis == 0) {
            result.track = Rect{ viewport.x, viewport.bottom() - thickness, viewport.width - corner, thickness };
        } else {
            result.track = Rect{ viewport.right() - thickness, viewport.y, thickness, viewport.height - corner };
        }

        int trackLength = axis == 0 ? result.track.width : result.track.height;
        int thumbLength = std::min(trackLength, std::max(MinimumThumb, trackLength * visibleSize / content));
        int thumbOffset = (trackLength - thumbLength) * scroll / std::max(1, content - visibleSize);

        // A small gap around the thumb, which thin bars can't afford
        int inset = thickness >= 8 ? 2 : 0;
        if (axis == 0) {
            result.thumb = Rect{ result.track.x + thumbOffset, result.track.y + inset, thumbLength, thickness - inset * 2 };
        } else {
            result.thumb = Rect{ result.track.x + inset, result.track.y + thumbOffset, thickness - inset * 2, thumbLength };
        }
        return result;
    }

    // The content may have shrunk since the last scroll, so the position is kept valid here
    void ScrollView::paint(ICanvas& canvas, const Rect& frame) {
        clampScroll();
        ControlObject::paint(canvas, frame);
    }

    void ScrollView::paintOverlay(ICanvas& canvas, const Rect& frame) {
        for (int axis = 0; axis < 2; ++axis) {
            Bar b = bar(axis);
            if (b.visible) canvas.fillRect(b.thumb, scrollBarColor, std::min(3, scrollBarThickness / 2));
        }
    }

    bool ScrollView::capturesPoint(int x, int y) const {
        for (int axis = 0; axis < 2; ++axis) {
            Bar b = bar(axis);
            if (b.visible && b.track.contains(x, y)) return true;
        }
        return false;
    }

    // Pressing a track jumps the thumb there, then dragging moves it
    void ScrollView::onMouseDown(int x, int y) {
        for (int axis = 0; axis < 2; ++axis) {
            Bar b = bar(axis);
            if (!b.visible || !b.track.contains(x, y)) continue;

            if (!b.thumb.contains(x, y)) {
                Rect viewport = innerFrame();
                int page = axis == 0 ? viewport.width : viewport.height;
                bool before = axis == 0 ? x < b.thumb.x : y < b.thumb.y;
                (axis == 0 ? scrollX : scrollY) += before ? -page : page;
                clampScroll();
                invalidate();
                return;
            }

            dragAxis = axis;
            dragStart = axis == 0 ? x : y;
            dragScrollStart = axis == 0 ? scrollX : scrollY;
            return;
        }
    }

    void ScrollView::onMouseMove(int x, int y) {
        if (dragAxis < 0) return;

        Bar b = bar(dragAxis);
        if (!b.visible) return;

        Rect viewport = innerFrame();
        int content = dragAxis == 0 ? contentSizeX() : contentSizeY();
        int visibleSize = dragAxis == 0 ? viewport.width : viewport.height;
        int trackLength = dragAxis == 0 ? b.track.width : b.track.height;
        int thumbLength = dragAxis == 0 ? b.thumb.width : b.thumb.height;
        int travel = std::max(1, trackLength - thumbLength);

        int moved = (dragAxis == 0 ? x : y) - dragStart;
        (dragAxis == 0 ? scrollX : scrollY) = dragScrollStart + moved * (content - visibleSize) / travel;
        clampScroll();
        invalidate();
    }

    void ScrollView::onMouseUp(int x, int y, bool inside) {
        dragAxis = -1;
    }

    // Positive amounts move the content toward the start, like a wheel turned up
    bool ScrollView::onWheel(int dx, int dy) {
        int oldX = scrollX;
        int oldY = scrollY;
        scrollX -= dx;
        scrollY -= dy;
        clampScroll();

        if (scrollX == oldX && scrollY == oldY) return false;
        invalidate();
        return true;
    }

    // Luau. The layout runs before measuring, so content added since the last paint counts.

    void ScrollView::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        ControlObject::readDescriptor(L);
        if (field(L, "ContentSize")) {
            checkSizeVector(L, -1, contentWidth, contentHeight);
            lua_pop(L, 1);
        }
        readEnum(L, "AutomaticCanvasSize", automaticCanvasSize, AutomaticSizeNames);
        readColor(L, "ScrollBarColor", scrollBarColor);
        readInt(L, "ScrollBarThickness", scrollBarThickness);
        scrollBarThickness = std::max(0, scrollBarThickness);
    }

    static int getScrollPosition(lua_State* L, ScrollView& object) {
        ControlLuau::pushVector2(L, static_cast<float>(object.scrollX), static_cast<float>(object.scrollY));
        return 1;
    }

    static int setScrollPosition(lua_State* L, ScrollView& object) {
        const float* v = luaL_checkvector(L, 3);
        object.view->update();
        object.scrollX = static_cast<int>(v[0]);
        object.scrollY = static_cast<int>(v[1]);
        object.clampScroll();
        object.invalidate();
        return 0;
    }

    // The measured size when ContentSize was left at 0
    static int getContentSize(lua_State* L, ScrollView& object) {
        object.view->update();
        ControlLuau::pushVector2(L, static_cast<float>(object.contentSizeX()), static_cast<float>(object.contentSizeY()));
        return 1;
    }

    static int setContentSize(lua_State* L, ScrollView& object) {
        ControlLuau::checkSizeVector(L, 3, object.contentWidth, object.contentHeight);
        object.view->update();
        object.clampScroll();
        object.invalidate();
        return 0;
    }

    static int getScrollBarColor(lua_State* L, ScrollView& object) {
        ControlLuau::pushColor(L, object.scrollBarColor);
        return 1;
    }

    static int setScrollBarColor(lua_State* L, ScrollView& object) {
        object.scrollBarColor = ControlLuau::checkColor(L, 3).value_or(0xA0A0A0);
        object.invalidate();
        return 0;
    }

    // 0 hides the bars, the content still scrolls with the wheel
    static int getScrollBarThickness(lua_State* L, ScrollView& object) {
        lua_pushinteger(L, object.scrollBarThickness);
        return 1;
    }

    static int setScrollBarThickness(lua_State* L, ScrollView& object) {
        object.scrollBarThickness = ControlLuau::checkSize(L, 3);
        object.invalidate();
        return 0;
    }

    // "None", "X", "Y" or "XY"
    static int getAutomaticCanvasSize(lua_State* L, ScrollView& object) {
        ControlLuau::pushEnum(L, object.automaticCanvasSize, AutomaticSizeNames);
        return 1;
    }

    static int setAutomaticCanvasSize(lua_State* L, ScrollView& object) {
        object.automaticCanvasSize = ControlLuau::checkEnum<AutomaticSize>(L, 3, AutomaticSizeNames);
        object.clampScroll();
        object.invalidate();
        return 0;
    }

    void ScrollView::RegisterClass(lua_State* L) {
        RegisterGuiObject<ScrollView>(L, {}, ControlLuau::extend<ScrollView>(ControlObject::properties<ScrollView>(), {
            { "ScrollPosition", Bind<getScrollPosition>, Bind<setScrollPosition> },
            { "ContentSize", Bind<getContentSize>, Bind<setContentSize> },
            { "AutomaticCanvasSize", Bind<getAutomaticCanvasSize>, Bind<setAutomaticCanvasSize> },
            { "ScrollBarColor", Bind<getScrollBarColor>, Bind<setScrollBarColor> },
            { "ScrollBarThickness", Bind<getScrollBarThickness>, Bind<setScrollBarThickness> },
        }));
    }
} // namespace Luwow::Udc
