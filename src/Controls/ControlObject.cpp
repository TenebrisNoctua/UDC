#include "ControlObject.h"
#include "ControlView.h"
#include "ControlRegistry.h"
#include "Corner.h"

#include "RectCore.h"

#include "Gradient.h"
#include "ListLayout.h"
#include "Padding.h"

#include <algorithm>
#include <cmath>

namespace Luwow::Udc {
    const char* const ItemLineAlignmentNames[] = { "Automatic", "Start", "Center", "End", "Stretch", nullptr };
    static const char* const flexModeNames[] = { "None", "Grow", "Shrink", "Fill", "Custom", nullptr };
    const char* const AutomaticSizeNames[] = { "None", "X", "Y", "XY", nullptr };

    static int toPixels(float value) {
        return static_cast<int>(std::lround(value));
    }

    float FlexItem::grow() const {
        switch (mode) {
            case FlexMode::Grow:
            case FlexMode::Fill: return 1;
            case FlexMode::Custom: return growRatio;
            default: return 0;
        }
    }

    float FlexItem::shrink() const {
        switch (mode) {
            case FlexMode::Shrink:
            case FlexMode::Fill: return 1;
            case FlexMode::Custom: return shrinkRatio;
            default: return 0;
        }
    }

    void ControlObject::destroy(lua_State* L) {
        if (view) view->destroy(this, L);
        else BaseObject::destroy(L);
    }

    void ControlObject::invalidate() {
        if (settingProperty > 0) {
            invalidatedWhileSetting = true;
            return;
        }
        if (view) view->invalidate();
    }

    void ControlObject::beginPropertySet() {
        if (settingProperty++ == 0) invalidatedWhileSetting = false;
    }

    // A set that changed nothing doesn't redraw, whatever the setter asked for
    void ControlObject::endPropertySet(bool changed) {
        if (--settingProperty > 0) return;
        bool redraw = invalidatedWhileSetting && changed;
        invalidatedWhileSetting = false;
        if (redraw) invalidate();
    }

    void ControlObject::propertyChanged(lua_State* L, std::string_view property) {
        invalidate();
        BaseGuiObject::propertyChanged(L, property);
    }

    BaseGuiObject* ControlObject::parentObject() const {
        if (!parent) return nullptr;
        if (view && parent == &view->root()) return view->owner;
        return parent;
    }

    std::vector<BaseGuiObject*> ControlObject::childObjects() const {
        return std::vector<BaseGuiObject*>(children.begin(), children.end());
    }

    void ControlObject::setParentObject(lua_State* L, int index) {
        if (lua_isnil(L, index)) luaL_error(L, "a %s's Parent can't be nil, Destroy removes it", getClassName());
        view->move(this, checkParent(L, index), L);
    }

    void ControlObject::propertySignalAdded() {
        if (view) view->watch(*this);
    }

    // The min size wins over the max size when they conflict
    void ControlObject::contentInsets(float width, float height, int& horizontal, int& vertical) const {
        int border = (borderColor && borderWidth > 0) ? borderWidth * 2 : 0;
        horizontal = border;
        vertical = border;
        if (const ControlObject* padding = findHelper(ControlKind::Padding)) {
            int sides, ends;
            static_cast<const Padding*>(padding)->insets(std::max(0.0f, width - border), std::max(0.0f, height - border), sides, ends);
            horizontal += std::max(0, sides);
            vertical += std::max(0, ends);
        }
    }

    // A ListLayout measures what it arranges, otherwise the children's furthest edges are used. Children
    // sized by scale take their share of the content area, so they don't grow it further.
    void ControlObject::measureContent(float width, float height, int& contentWidth, int& contentHeight) const {
        contentWidth = 0;
        contentHeight = 0;
        if (const ControlObject* layout = findHelper(ControlKind::ListLayout)) {
            static_cast<const ListLayout*>(layout)->measure(*this, width, height, contentWidth, contentHeight);
            return;
        }
        for (const ControlObject* child : children) {
            if (!child->visible || child->isHelper()) continue;
            int w, h;
            child->measuredSize(width, height, w, h);
            int x = toPixels(child->position.x.resolve(width) - child->anchorX * w);
            int y = toPixels(child->position.y.resolve(height) - child->anchorY * h);
            contentWidth = std::max(contentWidth, x + w);
            contentHeight = std::max(contentHeight, y + h);
        }
    }

    bool ControlObject::recallSize(SizeMemo* memos, float parentWidth, float parentHeight, int& width, int& height) const {
        if (!view) return false;
        for (int i = 0; i < 2; ++i) {
            const SizeMemo& memo = memos[i];
            if (memo.view == view && memo.generation == view->frameGeneration() && memo.parentWidth == parentWidth &&
                memo.parentHeight == parentHeight) {
                width = memo.width;
                height = memo.height;
                // Text measured after this wraps where it did when the size was worked out
                wrapLimit = memo.wrapLimit;
                return true;
            }
        }
        return false;
    }

    void ControlObject::rememberSize(SizeMemo* memos, uint8_t& next, float parentWidth, float parentHeight, int width, int height) const {
        if (!view) return;
        memos[next] = SizeMemo{ view, view->frameGeneration(), parentWidth, parentHeight, width, height, wrapLimit };
        next = static_cast<uint8_t>((next + 1) % 2);
    }

    void ControlObject::measuredSize(float parentWidth, float parentHeight, int& width, int& height) const {
        if (recallSize(measuredSizes, parentWidth, parentHeight, width, height)) return;
        measuredSizeUncached(parentWidth, parentHeight, width, height);
        rememberSize(measuredSizes, nextMeasured, parentWidth, parentHeight, width, height);
    }

    void ControlObject::resolveSize(float parentWidth, float parentHeight, int& width, int& height) const {
        if (recallSize(resolvedSizes, parentWidth, parentHeight, width, height)) return;
        resolveSizeUncached(parentWidth, parentHeight, width, height);
        rememberSize(resolvedSizes, nextResolved, parentWidth, parentHeight, width, height);
    }

    void ControlObject::measuredSizeUncached(float parentWidth, float parentHeight, int& width, int& height) const {
        resolveSize(parentWidth, parentHeight, width, height);
        bool scaledX = size.x.scale != 0;
        bool scaledY = size.y.scale != 0;
        if (!scaledX && !scaledY) return;

        int horizontal, vertical;
        contentInsets(static_cast<float>(width), static_cast<float>(height), horizontal, vertical);
        int contentWidth, contentHeight;
        measureContent(static_cast<float>(std::max(0, width - horizontal)), static_cast<float>(std::max(0, height - vertical)), contentWidth, contentHeight);
        if (scaledX) width = std::max(width, contentWidth + horizontal + toPixels(size.x.offset));
        if (scaledY) height = std::max(height, contentHeight + vertical + toPixels(size.y.offset));
    }

    void ControlObject::resolveSizeUncached(float parentWidth, float parentHeight, int& width, int& height) const {
        float w = size.x.resolve(parentWidth);
        float h = size.y.resolve(parentHeight);
        if (automaticSize != AutomaticSize::None) {
            int horizontal, vertical;
            contentInsets(w, h, horizontal, vertical);
            // Growing sideways up to MaxSize, wrapped text wraps at that width
            wrapLimit = (growsX(automaticSize) && maxSize) ? std::max(0, toPixels(maxSize->x.resolve(parentWidth)) - horizontal) : 0;
            int contentWidth, contentHeight;
            measureContent(std::max(0.0f, w - horizontal), std::max(0.0f, h - vertical), contentWidth, contentHeight);
            if (growsX(automaticSize)) w = std::max(w, static_cast<float>(contentWidth + horizontal));
            if (growsY(automaticSize)) h = std::max(h, static_cast<float>(contentHeight + vertical));
        }
        if (maxSize) {
            w = std::min(w, maxSize->x.resolve(parentWidth));
            h = std::min(h, maxSize->y.resolve(parentHeight));
        }
        if (minSize) {
            w = std::max(w, minSize->x.resolve(parentWidth));
            h = std::max(h, minSize->y.resolve(parentHeight));
        }
        if (aspectRatio && *aspectRatio > 0 && w > 0 && h > 0) {
            if (w / h > *aspectRatio) w = h * *aspectRatio;
            else h = w / *aspectRatio;
        }
        width = std::max(0, toPixels(w));
        height = std::max(0, toPixels(h));
    }

    // Scales are fractions of the parent's inner size. The root has no parent, so only its offsets count.
    Rect ControlObject::localFrame() const {
        return localFrameIn(parent ? parent->contentFrame() : Rect{});
    }

    Rect ControlObject::localFrameIn(const Rect& parentContent) const {
        float parentWidth = static_cast<float>(parentContent.width);
        float parentHeight = static_cast<float>(parentContent.height);

        int w, h;
        resolveSize(parentWidth, parentHeight, w, h);
        if (layoutWidth) w = *layoutWidth;
        if (layoutHeight) h = *layoutHeight;

        int x = layoutLeft ? *layoutLeft : toPixels(position.x.resolve(parentWidth) - anchorX * w);
        int y = layoutTop ? *layoutTop : toPixels(position.y.resolve(parentHeight) - anchorY * h);
        return Rect{ x, y, w, h };
    }

    Rect ControlObject::frame() const {
        bool caching = view && !view->isLayingOut();
        if (caching && cachedFrameGeneration == view->frameGeneration()) return cachedFrame;

        Rect result;
        if (parent) {
            Rect area = parent->contentFrame();
            Rect local = localFrameIn(area);
            int originX = 0;
            int originY = 0;
            parent->childOriginIn(area, originX, originY);
            result = Rect{ originX + local.x, originY + local.y, local.width, local.height };
        } else {
            result = localFrameIn(Rect{});
        }

        if (caching) {
            cachedFrame = result;
            cachedFrameGeneration = view->frameGeneration();
        }
        return result;
    }

    Rect ControlObject::innerFrame() const {
        Rect f = frame();
        if (!borderColor || borderWidth <= 0) return f;

        int inset = std::min(borderWidth, std::min(f.width, f.height) / 2);
        return Rect{ f.x + inset, f.y + inset, f.width - inset * 2, f.height - inset * 2 };
    }

    Rect ControlObject::contentFrame() const {
        Rect area = innerFrame();
        if (const ControlObject* padding = findHelper(ControlKind::Padding)) return static_cast<const Padding*>(padding)->inset(area);
        return area;
    }

    // Children are placed inside the border and the padding
    void ControlObject::childOrigin(int& x, int& y) const {
        childOriginIn(contentFrame(), x, y);
    }

    void ControlObject::childOriginIn(const Rect& content, int& x, int& y) const {
        x = content.x;
        y = content.y;
    }

    // Siblings with the same ZIndex keep the order they were added in
    std::vector<ControlObject*> ControlObject::drawOrder() const {
        std::vector<ControlObject*> ordered = children;
        std::stable_sort(ordered.begin(), ordered.end(), [](const ControlObject* a, const ControlObject* b) {
            return a->zIndex < b->zIndex;
        });
        return ordered;
    }

    const ControlObject* ControlObject::findHelper(ControlKind helperKind) const {
        for (const ControlObject* child : children) {
            if (child->kind == helperKind) return child;
        }
        return nullptr;
    }

    CornerRadii ControlObject::resolveCornerRadii(const Rect& frame) const {
        if (const ControlObject* corner = findHelper(ControlKind::Corner)) return static_cast<const Corner*>(corner)->resolve(frame);
        return CornerRadii();
    }

    void ControlObject::paintBox(ICanvas& canvas, const Rect& frame, std::optional<uint32_t> background, std::optional<uint32_t> border) {
        CornerRadii radii = resolveCornerRadii(frame);
        // A Gradient child colors the background along its line
        const ControlObject* gradient = findHelper(ControlKind::Gradient);
        if (background && gradient && static_cast<const Gradient*>(gradient)->enabled) {
            const auto* colors = static_cast<const Gradient*>(gradient);
            canvas.fillGradient(frame, colors->stops(*background), colors->rotation, radii);
        } else if (background) {
            canvas.fillRect(frame, *background, radii);
        }
        if (border && borderWidth > 0) canvas.strokeRect(frame, *border, borderWidth, radii);
    }

    bool ControlObject::activeByDefault() const {
        for (const char* event : { "OnEnter", "OnLeft", "OnPressed", "OnUnPressed", "OnInputBegan", "OnInputEnded" }) {
            if (handlers.count(event)) return true;
        }
        return false;
    }

    void ControlObject::paint(ICanvas& canvas, const Rect& frame) {
        paintBox(canvas, frame, backgroundColor, borderColor);
    }

    // Luau

    void ControlObject::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        readString(L, "Name", name);
        if (isHelper()) return;

        readUDim2(L, "Position", position);
        readUDim2(L, "Size", size);
        if (field(L, "AnchorPoint")) {
            const float* v = luaL_checkvector(L, -1);
            anchorX = v[0];
            anchorY = v[1];
            lua_pop(L, 1);
        }
        readOptionalUDim2(L, "MinSize", minSize);
        readOptionalUDim2(L, "MaxSize", maxSize);
        if (field(L, "AspectRatio")) {
            Luau::setAspectRatioAt(L, *this, -1);
            lua_pop(L, 1);
        }
        if (field(L, "Flex")) {
            Luau::setFlexAt(L, *this, -1);
            lua_pop(L, 1);
        }
        readEnum(L, "AutomaticSize", automaticSize, AutomaticSizeNames);
        readBool(L, "Visible", visible);
        readBool(L, "ClipsDescendants", clipsDescendants);
        if (field(L, "Active")) {
            luaL_checktype(L, -1, LUA_TBOOLEAN);
            activeSetting = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }
        readInt(L, "LayoutOrder", layoutOrder);
        readInt(L, "ZIndex", zIndex);
        readColor(L, "BackgroundColor", backgroundColor);
        readColor(L, "BorderColor", borderColor);
        readInt(L, "BorderWidth", borderWidth);

        // The pointer events every drawn control has
        readHandler(L, "OnEnter", *this);
        readHandler(L, "OnLeft", *this);
        readHandler(L, "OnPressed", *this);
        readHandler(L, "OnUnPressed", *this);
        readHandler(L, "OnInputBegan", *this);
        readHandler(L, "OnInputEnded", *this);
    }

    // Position and Size take a UDim2, or a vector of pixels

    int ControlObject::Luau::getPosition(lua_State* L, ControlObject& object) {
        Luwow::UDims::pushUDim2(L, object.position);
        return 1;
    }

    int ControlObject::Luau::setPosition(lua_State* L, ControlObject& object) {
        object.position = Luwow::UDims::checkUDim2Like(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getSize(lua_State* L, ControlObject& object) {
        Luwow::UDims::pushUDim2(L, object.size);
        return 1;
    }

    int ControlObject::Luau::setSize(lua_State* L, ControlObject& object) {
        object.size = Luwow::UDims::checkUDim2Like(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getAnchorPoint(lua_State* L, ControlObject& object) {
        ControlLuau::pushVector2(L, object.anchorX, object.anchorY);
        return 1;
    }

    int ControlObject::Luau::setAnchorPoint(lua_State* L, ControlObject& object) {
        const float* v = luaL_checkvector(L, 3);
        object.anchorX = v[0];
        object.anchorY = v[1];
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getMinSize(lua_State* L, ControlObject& object) {
        ControlLuau::pushOptionalUDim2(L, object.minSize);
        return 1;
    }

    int ControlObject::Luau::setMinSize(lua_State* L, ControlObject& object) {
        object.minSize = ControlLuau::checkOptionalUDim2(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getMaxSize(lua_State* L, ControlObject& object) {
        ControlLuau::pushOptionalUDim2(L, object.maxSize);
        return 1;
    }

    int ControlObject::Luau::setMaxSize(lua_State* L, ControlObject& object) {
        object.maxSize = ControlLuau::checkOptionalUDim2(L, 3);
        object.invalidate();
        return 0;
    }

    // A positive number, nil or false for none
    void ControlObject::Luau::setAspectRatioAt(lua_State* L, ControlObject& object, int index) {
        if (lua_isnoneornil(L, index) || (lua_isboolean(L, index) && !lua_toboolean(L, index))) {
            object.aspectRatio.reset();
            return;
        }
        float ratio = static_cast<float>(luaL_checknumber(L, index));
        if (!(ratio > 0)) luaL_argerror(L, index, "must be positive");
        object.aspectRatio = ratio;
    }

    int ControlObject::Luau::getAspectRatio(lua_State* L, ControlObject& object) {
        if (object.aspectRatio) lua_pushnumber(L, *object.aspectRatio);
        else lua_pushnil(L);
        return 1;
    }

    int ControlObject::Luau::setAspectRatio(lua_State* L, ControlObject& object) {
        setAspectRatioAt(L, object, 3);
        object.invalidate();
        return 0;
    }

    static float checkRatio(lua_State* L, int table, const char* name) {
        lua_getfield(L, table, name);
        float ratio = lua_isnil(L, -1) ? 0.0f : static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        if (ratio < 0) luaL_error(L, "Flex's %s can't be negative", name);
        return ratio;
    }

    // nil or false for none, a FlexMode name, or a table of Mode, GrowRatio, ShrinkRatio and
    // ItemLineAlignment. A table with ratios and no Mode is Custom.
    void ControlObject::Luau::setFlexAt(lua_State* L, ControlObject& object, int index) {
        index = lua_absindex(L, index);
        std::optional<FlexItem> flex;
        if (lua_isstring(L, index)) {
            flex = FlexItem{ ControlLuau::checkEnum<FlexMode>(L, index, flexModeNames) };
        } else if (!lua_isnoneornil(L, index) && !(lua_isboolean(L, index) && !lua_toboolean(L, index))) {
            luaL_checktype(L, index, LUA_TTABLE);
            FlexItem item;
            item.growRatio = checkRatio(L, index, "GrowRatio");
            item.shrinkRatio = checkRatio(L, index, "ShrinkRatio");
            lua_getfield(L, index, "Mode");
            if (!lua_isnil(L, -1)) item.mode = ControlLuau::checkEnum<FlexMode>(L, -1, flexModeNames);
            else if (item.growRatio > 0 || item.shrinkRatio > 0) item.mode = FlexMode::Custom;
            lua_pop(L, 1);
            lua_getfield(L, index, "ItemLineAlignment");
            if (!lua_isnil(L, -1)) item.itemLineAlignment = ControlLuau::checkEnum<ItemLineAlignment>(L, -1, ItemLineAlignmentNames);
            lua_pop(L, 1);
            flex = item;
        }

        if (object.flexTable != LUA_NOREF) lua_unref(L, object.flexTable);
        object.flexTable = LUA_NOREF;
        object.flex = flex;
        if (!flex) return;

        // What scripts read back, with the ratios each mode uses
        lua_createtable(L, 0, 4);
        ControlLuau::pushEnum(L, flex->mode, flexModeNames);
        lua_setfield(L, -2, "Mode");
        lua_pushnumber(L, flex->grow());
        lua_setfield(L, -2, "GrowRatio");
        lua_pushnumber(L, flex->shrink());
        lua_setfield(L, -2, "ShrinkRatio");
        if (flex->itemLineAlignment) {
            ControlLuau::pushEnum(L, *flex->itemLineAlignment, ItemLineAlignmentNames);
            lua_setfield(L, -2, "ItemLineAlignment");
        }
        lua_setreadonly(L, -1, true);
        object.flexTable = lua_ref(L, -1);
        lua_pop(L, 1);
    }

    int ControlObject::Luau::getFlex(lua_State* L, ControlObject& object) {
        if (object.flexTable != LUA_NOREF) lua_getref(L, object.flexTable);
        else lua_pushnil(L);
        return 1;
    }

    int ControlObject::Luau::setFlex(lua_State* L, ControlObject& object) {
        setFlexAt(L, object, 3);
        object.invalidate();
        return 0;
    }

    // "None", "X", "Y" or "XY"
    int ControlObject::Luau::getAutomaticSize(lua_State* L, ControlObject& object) {
        ControlLuau::pushEnum(L, object.automaticSize, AutomaticSizeNames);
        return 1;
    }

    int ControlObject::Luau::setAutomaticSize(lua_State* L, ControlObject& object) {
        object.automaticSize = ControlLuau::checkEnum<AutomaticSize>(L, 3, AutomaticSizeNames);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getVisible(lua_State* L, ControlObject& object) {
        lua_pushboolean(L, object.visible);
        return 1;
    }

    // A hidden control can't keep the focus
    int ControlObject::Luau::setVisible(lua_State* L, ControlObject& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.visible = lua_toboolean(L, 3);
        if (!object.visible && object.focused) object.view->setFocus(nullptr);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getClipsDescendants(lua_State* L, ControlObject& object) {
        lua_pushboolean(L, object.clips());
        return 1;
    }

    int ControlObject::Luau::setClipsDescendants(lua_State* L, ControlObject& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.clipsDescendants = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getActive(lua_State* L, ControlObject& object) {
        lua_pushboolean(L, object.active());
        return 1;
    }

    // nil goes back to the control's default
    int ControlObject::Luau::setActive(lua_State* L, ControlObject& object) {
        if (lua_isnil(L, 3)) {
            object.activeSetting.reset();
        } else {
            luaL_checktype(L, 3, LUA_TBOOLEAN);
            object.activeSetting = lua_toboolean(L, 3);
        }
        return 0;
    }

    int ControlObject::Luau::getLayoutOrder(lua_State* L, ControlObject& object) {
        lua_pushinteger(L, object.layoutOrder);
        return 1;
    }

    int ControlObject::Luau::setLayoutOrder(lua_State* L, ControlObject& object) {
        object.layoutOrder = luaL_checkinteger(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getZIndex(lua_State* L, ControlObject& object) {
        lua_pushinteger(L, object.zIndex);
        return 1;
    }

    int ControlObject::Luau::setZIndex(lua_State* L, ControlObject& object) {
        object.zIndex = luaL_checkinteger(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getBackgroundColor(lua_State* L, ControlObject& object) {
        ControlLuau::pushColor(L, object.backgroundColor);
        return 1;
    }

    int ControlObject::Luau::setBackgroundColor(lua_State* L, ControlObject& object) {
        object.backgroundColor = ControlLuau::checkColor(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getBorderColor(lua_State* L, ControlObject& object) {
        ControlLuau::pushColor(L, object.borderColor);
        return 1;
    }

    int ControlObject::Luau::setBorderColor(lua_State* L, ControlObject& object) {
        object.borderColor = ControlLuau::checkColor(L, 3);
        object.invalidate();
        return 0;
    }

    int ControlObject::Luau::getBorderWidth(lua_State* L, ControlObject& object) {
        lua_pushinteger(L, object.borderWidth);
        return 1;
    }

    int ControlObject::Luau::setBorderWidth(lua_State* L, ControlObject& object) {
        object.borderWidth = ControlLuau::checkSize(L, 3);
        object.invalidate();
        return 0;
    }

    // Where the control is in the window, after layouts and scrolling.

    int ControlObject::Luau::getAbsolutePosition(lua_State* L, ControlObject& object) {
        object.view->update();
        Rect frame = object.frame();
        ControlLuau::pushVector2(L, static_cast<float>(frame.x), static_cast<float>(frame.y));
        return 1;
    }

    int ControlObject::Luau::getAbsoluteSize(lua_State* L, ControlObject& object) {
        object.view->update();
        Rect frame = object.frame();
        ControlLuau::pushVector2(L, static_cast<float>(frame.width), static_cast<float>(frame.height));
        return 1;
    }

    int ControlObject::Luau::getAbsoluteFrame(lua_State* L, ControlObject& object) {
        object.view->update();
        Luwow::Rects::pushRect(L, object.frame());
        return 1;
    }
} // namespace Luwow::Udc
