#pragma once

#include "BaseGuiObject.h"
#include "Canvas.h"
#include "ControlLuau.h"
#include "UDimCore.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Luwow::Udc {
    class ControlView;

    using UDim = Luwow::UDims::UDim;
    using UDim2 = Luwow::UDims::UDim2;

    enum class ControlKind {
        View,
        ScrollView,
        TextLabel,
        TextButton,
        TextInput,
        ListLayout,
        Corner,
        ImageLabel,
        Padding,
        Stroke,
        ImageButton,
        Gradient
    };

    // How items sit across their line in a ListLayout
    enum class ItemLineAlignment { Automatic, Start, Center, End, Stretch };
    // Their names in Luau, in the enum's order
    extern const char* const ItemLineAlignmentNames[];

    enum class FlexMode { None, Grow, Shrink, Fill, Custom };

    // The axes a size grows along to fit content
    enum class AutomaticSize { None, X, Y, XY };
    // Their names in Luau, in the enum's order
    extern const char* const AutomaticSizeNames[];

    inline bool growsX(AutomaticSize size) { return size == AutomaticSize::X || size == AutomaticSize::XY; }
    inline bool growsY(AutomaticSize size) { return size == AutomaticSize::Y || size == AutomaticSize::XY; }

    // How a control grows into and shrinks out of its line in a ListLayout.
    struct FlexItem {
        FlexMode mode = FlexMode::None;
        // Only used by Custom
        float growRatio = 0;
        float shrinkRatio = 0;
        // Overrides the layout's ItemLineAlignment for this control
        std::optional<ItemLineAlignment> itemLineAlignment;

        float grow() const;
        float shrink() const;
    };

    class ControlObject : public BaseGuiObject {
    public:
        static constexpr const char* Name = "ControlObject";
        using Super = BaseGuiObject;

        explicit ControlObject(ControlKind kind) : kind(kind) {}

        const ControlKind kind;
        uint64_t id = 0;
        ControlView* view = nullptr;
        ControlObject* parent = nullptr;
        std::vector<ControlObject*> children;

        UDim2 position;
        UDim2 size = UDim2::fromScale(1, 1);

        float anchorX = 0;
        float anchorY = 0;
        // Limits on the resolved size, the min size wins when they conflict
        std::optional<UDim2> minSize;
        std::optional<UDim2> maxSize;
        // Width over height. The size is fitted within the resolved size.
        std::optional<float> aspectRatio;
        // How a ListLayout parent grows and shrinks the control
        std::optional<FlexItem> flex;
        // Grows the size to fit the content, never shrinking it below its own size
        AutomaticSize automaticSize = AutomaticSize::None;
        // Flex as scripts read it, a frozen table kept as a registry reference so reads give the same table
        int flexTable = -1;
        bool visible = true;
        // Whether the children are cut off at the control's edges
        bool clipsDescendants = true;

        // BaseObject: while a property is set, the setter's redraw waits until it's known to be a change
        void beginPropertySet() override;
        void endPropertySet(bool changed) override;

        // Set by a layout in the parent, they take precedence over the position and size without
        // changing them, so removing the layout restores the control's own placement
        std::optional<int> layoutLeft;
        std::optional<int> layoutTop;
        std::optional<int> layoutWidth;
        std::optional<int> layoutHeight;

        // Used by layouts to order their container's children, along with the name
        int layoutOrder = 0;
        // The order among siblings, higher draws on top and gets the mouse first. Equal ones keep the order they were added in.
        int zIndex = 1;

        // nullopt draws nothing
        std::optional<uint32_t> backgroundColor;
        std::optional<uint32_t> borderColor;
        int borderWidth = 1;

        // The frame last worked out, and the view's frame generation it's for
        mutable Rect cachedFrame;
        mutable uint64_t cachedFrameGeneration = 0;

        // A size worked out for a parent of a given size, while the view's frame generation hasn't changed.
        struct SizeMemo {
            const ControlView* view = nullptr;
            uint64_t generation = 0;
            float parentWidth = 0;
            float parentHeight = 0;
            int width = 0;
            int height = 0;
            int wrapLimit = 0;
        };
        // Two of each, for a parent's measuring pass and its own size
        mutable SizeMemo resolvedSizes[2];
        mutable SizeMemo measuredSizes[2];
        mutable uint8_t nextResolved = 0;
        mutable uint8_t nextMeasured = 0;

        bool recallSize(SizeMemo* memos, float parentWidth, float parentHeight, int& width, int& height) const;
        void rememberSize(SizeMemo* memos, uint8_t& next, float parentWidth, float parentHeight, int width, int height) const;
        void measuredSizeUncached(float parentWidth, float parentHeight, int& width, int& height) const;
        void resolveSizeUncached(float parentWidth, float parentHeight, int& width, int& height) const;

        // Nested property sets in progress, and whether one asked for a redraw meanwhile
        int settingProperty = 0;
        bool invalidatedWhileSetting = false;

        // Luau handlers by event name, as registry references
        std::unordered_map<std::string, int> handlers;

        bool hovered = false;
        bool pressed = false;
        bool focused = false;

        // Helpers such as layouts aren't drawn, hit or laid out themselves, and can't have children
        virtual bool isHelper() const { return false; }

        // The children from back to front: by ZIndex, then in the order they were added
        std::vector<ControlObject*> drawOrder() const;

        // The corner radii in pixels for a frame
        CornerRadii resolveCornerRadii(const Rect& frame) const;

        // The first child of a helper kind, such as a Corner or a Padding
        const ControlObject* findHelper(ControlKind helperKind) const;

        // The size in pixels within a parent of the given size, grown to fit the content for AutomaticSize,
        // with the min and max sizes and the aspect ratio applied
        void resolveSize(float parentWidth, float parentHeight, int& width, int& height) const;

        // The size the control takes when its parent measures its content, its resolved size, and along an
        // axis sized by scale, at least what its own content takes plus its offset
        void measuredSize(float parentWidth, float parentHeight, int& width, int& height) const;

        // The size the control's content takes when its content area is width by height, the children's
        // furthest edges, or what its ListLayout arranges.
        virtual void measureContent(float width, float height, int& contentWidth, int& contentHeight) const;

        // The width wrapped text measures at while the control grows sideways, from its MaxSize.
        mutable int wrapLimit = 0;

        // The space the border and the padding take around the content, for a frame of width by height
        void contentInsets(float width, float height, int& horizontal, int& vertical) const;

        // Position and size in pixels relative to the parent's content, with layout overrides applied
        Rect localFrame() const;
        // The same, for the parent's content frame already worked out
        Rect localFrameIn(const Rect& parentContent) const;

        // The control's rectangle in window coordinates, cached until the view's frames change
        Rect frame() const;

        // The frame without its border, children are clipped to it
        Rect innerFrame() const;

        // The inner frame without the Padding child's insets, children are placed and sized in it
        Rect contentFrame() const;

        // Where children's positions are measured from, in window coordinates
        void childOrigin(int& x, int& y) const;
        // The same, for the content frame already worked out, a frame asks its parent's content frame
        // once for both its size and its position, or each level would work out the ones above twice
        virtual void childOriginIn(const Rect& content, int& x, int& y) const;

        // Asks the window to paint again
        void invalidate();

        // Paints the control itself, its children are painted on top, clipped to its inner frame
        virtual void paint(ICanvas& canvas, const Rect& frame);
        // Paints over the children, such as scroll bars
        virtual void paintOverlay(ICanvas& canvas, const Rect& frame) {}
        // Paints what's still seen of a control with no width or height
        virtual void paintEmpty(ICanvas& canvas, const Rect& frame) {}

        // Whether a point belongs to the control itself even when a child is under it
        virtual bool capturesPoint(int x, int y) const { return false; }

        // Whether pointers hit the control, the top active control under a pointer
        // gets its events, and the ones that aren't let them through to what's behind. nullopt follows
        // activeByDefault.
        std::optional<bool> activeSetting;
        bool active() const { return activeSetting.value_or(activeByDefault()); }
        // Whether the children are cut off at the edges
        virtual bool clips() const { return clipsDescendants; }
        virtual bool activeByDefault() const;

        virtual bool focusable() const { return false; }

        // Input, in window coordinates.
        virtual void onMouseDown(int x, int y) {}
        virtual void onMouseMove(int x, int y) {}
        virtual void onMouseUp(int x, int y, bool inside) {}
        virtual void onHoverChanged() { invalidate(); }
        // Returns whether the control used the wheel, otherwise it goes to the parent
        virtual bool onWheel(int dx, int dy) { return false; }
        virtual void onKey(Key key, KeyModifiers modifiers) {}
        virtual void onText(const std::string& text) {}
        virtual void onFocusChanged() { invalidate(); }

        // Luau

        // Reads the descriptor table at index 1 when the control is created.
        virtual void readDescriptor(lua_State* L);

        // The Luau properties every drawn control has
        template <typename T>
        static ControlLuau::Properties<T> properties() {
            return {
                { "Position", T::template Bind<&Luau::getPosition>, T::template Bind<&Luau::setPosition> },
                { "Size", T::template Bind<&Luau::getSize>, T::template Bind<&Luau::setSize> },
                { "AnchorPoint", T::template Bind<&Luau::getAnchorPoint>, T::template Bind<&Luau::setAnchorPoint> },
                { "MinSize", T::template Bind<&Luau::getMinSize>, T::template Bind<&Luau::setMinSize> },
                { "MaxSize", T::template Bind<&Luau::getMaxSize>, T::template Bind<&Luau::setMaxSize> },
                { "AspectRatio", T::template Bind<&Luau::getAspectRatio>, T::template Bind<&Luau::setAspectRatio> },
                { "Flex", T::template Bind<&Luau::getFlex>, T::template Bind<&Luau::setFlex> },
                { "AutomaticSize", T::template Bind<&Luau::getAutomaticSize>, T::template Bind<&Luau::setAutomaticSize> },
                { "Visible", T::template Bind<&Luau::getVisible>, T::template Bind<&Luau::setVisible> },
                { "ClipsDescendants", T::template Bind<&Luau::getClipsDescendants>, T::template Bind<&Luau::setClipsDescendants> },
                { "Active", T::template Bind<&Luau::getActive>, T::template Bind<&Luau::setActive> },
                { "LayoutOrder", T::template Bind<&Luau::getLayoutOrder>, T::template Bind<&Luau::setLayoutOrder> },
                { "ZIndex", T::template Bind<&Luau::getZIndex>, T::template Bind<&Luau::setZIndex> },
                { "BackgroundColor", T::template Bind<&Luau::getBackgroundColor>, T::template Bind<&Luau::setBackgroundColor> },
                { "BorderColor", T::template Bind<&Luau::getBorderColor>, T::template Bind<&Luau::setBorderColor> },
                { "BorderWidth", T::template Bind<&Luau::getBorderWidth>, T::template Bind<&Luau::setBorderWidth> },
                { "AbsolutePosition", T::template Bind<&Luau::getAbsolutePosition> },
                { "AbsoluteSize", T::template Bind<&Luau::getAbsoluteSize> },
                { "AbsoluteFrame", T::template Bind<&Luau::getAbsoluteFrame> },
            };
        }

        // BaseObject

        // Destroys the control and its children through the view
        void destroy(lua_State* L) override;
        // Any property may change the drawing
        void propertyChanged(lua_State* L, std::string_view property) override;

        // BaseGuiObject

        // The window for the controls at the top, whose parent is the window's invisible root
        BaseGuiObject* parentObject() const override;
        std::vector<BaseGuiObject*> childObjects() const override;
        // Moves the control into a Window or a drawn control
        void setParentObject(lua_State* L, int index) override;

    protected:
        // The view checks the control's bound properties after each paint and input event
        void propertySignalAdded() override;

        // Paints the background and border, with the given colors instead of the control's own
        void paintBox(ICanvas& canvas, const Rect& frame, std::optional<uint32_t> background, std::optional<uint32_t> border);

    private:
        // The Luau properties
        struct Luau {
            static int getPosition(lua_State* L, ControlObject& object);
            static int setPosition(lua_State* L, ControlObject& object);
            static int getSize(lua_State* L, ControlObject& object);
            static int setSize(lua_State* L, ControlObject& object);
            static int getAnchorPoint(lua_State* L, ControlObject& object);
            static int setAnchorPoint(lua_State* L, ControlObject& object);
            static int getMinSize(lua_State* L, ControlObject& object);
            static int setMinSize(lua_State* L, ControlObject& object);
            static int getMaxSize(lua_State* L, ControlObject& object);
            static int setMaxSize(lua_State* L, ControlObject& object);
            static void setAspectRatioAt(lua_State* L, ControlObject& object, int index);
            static void setFlexAt(lua_State* L, ControlObject& object, int index);
            static int getAspectRatio(lua_State* L, ControlObject& object);
            static int setAspectRatio(lua_State* L, ControlObject& object);
            static int getFlex(lua_State* L, ControlObject& object);
            static int getAutomaticSize(lua_State* L, ControlObject& object);
            static int setAutomaticSize(lua_State* L, ControlObject& object);
            static int setFlex(lua_State* L, ControlObject& object);
            static int getVisible(lua_State* L, ControlObject& object);
            static int getActive(lua_State* L, ControlObject& object);
            static int setActive(lua_State* L, ControlObject& object);
            static int setVisible(lua_State* L, ControlObject& object);
            static int getClipsDescendants(lua_State* L, ControlObject& object);
            static int setClipsDescendants(lua_State* L, ControlObject& object);
            static int getLayoutOrder(lua_State* L, ControlObject& object);
            static int setLayoutOrder(lua_State* L, ControlObject& object);
            static int getZIndex(lua_State* L, ControlObject& object);
            static int setZIndex(lua_State* L, ControlObject& object);
            static int getBackgroundColor(lua_State* L, ControlObject& object);
            static int setBackgroundColor(lua_State* L, ControlObject& object);
            static int getBorderColor(lua_State* L, ControlObject& object);
            static int setBorderColor(lua_State* L, ControlObject& object);
            static int getBorderWidth(lua_State* L, ControlObject& object);
            static int setBorderWidth(lua_State* L, ControlObject& object);
            static int getAbsolutePosition(lua_State* L, ControlObject& object);
            static int getAbsoluteSize(lua_State* L, ControlObject& object);
            static int getAbsoluteFrame(lua_State* L, ControlObject& object);
        };
    };
} // namespace Luwow::Udc
