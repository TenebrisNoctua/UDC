#include "ListLayout.h"
#include "ControlView.h"

#include <algorithm>
#include <cmath>
#include <vector>

// Items flow along the fill direction (the main axis) and, when wrapping, form several lines stacked
// along the other one (the cross axis). The math works on main/cross values so both directions share it.
namespace Luwow::Udc {
    namespace {
        struct Item {
            ControlObject* control;
            int main;
            int cross;
            int mainPosition = 0;
            int crossPosition = 0;
        };

        struct Line {
            size_t first;
            size_t count;
            int main = 0;
            int cross = 0;
            int crossPosition = 0;
        };

        enum class Align { Start, Center, End };

        Align toAlign(HorizontalAlignment alignment) {
            if (alignment == HorizontalAlignment::Center) return Align::Center;
            if (alignment == HorizontalAlignment::Right) return Align::End;
            return Align::Start;
        }

        Align toAlign(VerticalAlignment alignment) {
            if (alignment == VerticalAlignment::Center) return Align::Center;
            if (alignment == VerticalAlignment::Bottom) return Align::End;
            return Align::Start;
        }

        // Where a block of size starts within available space.
        int alignOffset(Align align, int available, int size) {
            int free = available - size;
            if (free <= 0) return 0;
            if (align == Align::Center) return free / 2;
            if (align == Align::End) return free;
            return 0;
        }

        // Across a single line, an item bigger than the space overflows it evenly on both
        // sides when centered, and past its start when aligned to the end
        int crossOffset(Align align, int available, int size) {
            if (align == Align::Center) return (available - size) / 2;
            if (align == Align::End) return available - size;
            return 0;
        }

        // Shares free space among blocks separated by padding. Gives each block's offset from the start,
        // and grows the sizes for Fill.
        void distribute(FlexAlignment flex, Align align, int available, int padding, std::vector<int>& sizes, std::vector<int>& offsets) {
            size_t count = sizes.size();
            offsets.assign(count, 0);
            if (count == 0) return;

            int used = padding * static_cast<int>(count - 1);
            for (int size : sizes) used += size;
            int free = std::max(0, available - used);
            int n = static_cast<int>(count);

            int start = 0;
            int gap = padding;
            switch (flex) {
                case FlexAlignment::Fill:
                    for (size_t i = 0; i < count; ++i) {
                        // The remainder goes to the last block, so the line reaches the end exactly
                        sizes[i] += free / n + (i == count - 1 ? free % n : 0);
                    }
                    break;
                case FlexAlignment::SpaceAround:
                    start = free / (n * 2);
                    gap += free / n;
                    break;
                case FlexAlignment::SpaceBetween:
                    if (n > 1) gap += free / (n - 1);
                    else start = alignOffset(align, available, used);
                    break;
                case FlexAlignment::SpaceEvenly:
                    start = free / (n + 1);
                    gap += free / (n + 1);
                    break;
                case FlexAlignment::None:
                    start = alignOffset(align, available, used);
                    break;
            }

            int position = start;
            for (size_t i = 0; i < count; ++i) {
                offsets[i] = position;
                position += sizes[i] + gap;
            }
        }
    }

    // Free space goes to the ones that grow, by their grow ratios, and overflow is
    // taken from the ones that shrink, by their shrink ratios times their sizes.
    static void flexItems(const std::vector<Item>& items, size_t first, size_t count, int free, std::vector<int>& sizes) {
        if (free == 0) return;

        float total = 0;
        for (size_t i = 0; i < count; ++i) {
            const ControlObject& control = *items[first + i].control;
            if (!control.flex) continue;
            total += free > 0 ? control.flex->grow() : control.flex->shrink() * static_cast<float>(sizes[i]);
        }
        if (total <= 0) return;

        int given = 0;
        int last = -1;
        for (size_t i = 0; i < count; ++i) {
            const ControlObject& control = *items[first + i].control;
            if (!control.flex) continue;
            float weight = free > 0 ? control.flex->grow() : control.flex->shrink() * static_cast<float>(sizes[i]);
            if (weight <= 0) continue;
            int share = static_cast<int>(static_cast<float>(free) * weight / total);
            sizes[i] = std::max(0, sizes[i] + share);
            given += share;
            last = static_cast<int>(i);
        }

        // The rounding goes to the last one that takes part, so a growing line reaches the end exactly
        if (free > 0 && last >= 0) sizes[last] += free - given;
    }

    void ListLayout::measure(const ControlObject& container, float width, float height, int& contentWidth, int& contentHeight) const {
        bool horizontal = fillDirection == FillDirection::Horizontal;
        int gap = std::max(0, static_cast<int>(std::lround(padding.resolve(horizontal ? width : height))));
        int main = 0;
        int cross = 0;
        int count = 0;
        for (const ControlObject* child : container.children) {
            if (!child->visible || child->isHelper()) continue;
            int w, h;
            child->measuredSize(width, height, w, h);
            main += (horizontal ? w : h) + (count > 0 ? gap : 0);
            cross = std::max(cross, horizontal ? h : w);
            ++count;
        }
        contentWidth = horizontal ? main : cross;
        contentHeight = horizontal ? cross : main;
    }

    void ListLayout::arrange(ControlObject& container) {
        bool horizontal = fillDirection == FillDirection::Horizontal;
        Rect area = container.contentFrame();
        int availableMain = horizontal ? area.width : area.height;
        int availableCross = horizontal ? area.height : area.width;
        int gap = std::max(0, static_cast<int>(std::lround(padding.resolve(static_cast<float>(availableMain)))));

        // The items' own sizes, resolved within the container
        std::vector<Item> items;
        for (ControlObject* child : container.children) {
            if (!child->visible || child->isHelper()) continue;
            int w, h;
            child->resolveSize(static_cast<float>(area.width), static_cast<float>(area.height), w, h);
            items.push_back(Item{ child, horizontal ? w : h, horizontal ? h : w });
        }

        // Equal keys keep the order the children were added in
        std::stable_sort(items.begin(), items.end(), [this](const Item& a, const Item& b) {
            if (sortOrder == SortOrder::Name) return a.control->name < b.control->name;
            return a.control->layoutOrder < b.control->layoutOrder;
        });

        // Breaks the items into lines
        std::vector<Line> lines;
        for (size_t i = 0; i < items.size(); ++i) {
            if (lines.empty() || (wraps && lines.back().count > 0 && lines.back().main + gap + items[i].main > availableMain)) {
                lines.push_back(Line{ i, 0 });
            }

            Line& line = lines.back();
            line.main += (line.count > 0 ? gap : 0) + items[i].main;
            line.cross = std::max(line.cross, items[i].cross);
            ++line.count;
        }

        int contentMain = 0;
        int contentCross = 0;
        for (const Line& line : lines) {
            contentMain = std::max(contentMain, line.main);
            contentCross += line.cross;
        }
        if (!lines.empty()) contentCross += gap * static_cast<int>(lines.size() - 1);
        contentWidth = horizontal ? contentMain : contentCross;
        contentHeight = horizontal ? contentCross : contentMain;

        Align mainAlign = horizontal ? toAlign(horizontalAlignment) : toAlign(verticalAlignment);
        Align crossAlign = horizontal ? toAlign(verticalAlignment) : toAlign(horizontalAlignment);
        FlexAlignment mainFlex = horizontal ? horizontalFlex : verticalFlex;
        FlexAlignment crossFlex = horizontal ? verticalFlex : horizontalFlex;

        // Without wrapping the single line spans the container, so items align across all of it
        if (!wraps && lines.size() == 1) lines[0].cross = std::max(lines[0].cross, availableCross);

        // Places the lines along the cross axis
        std::vector<int> sizes;
        std::vector<int> offsets;
        for (const Line& line : lines) sizes.push_back(line.cross);
        distribute(wraps ? crossFlex : FlexAlignment::None, crossAlign, availableCross, gap, sizes, offsets);
        for (size_t l = 0; l < lines.size(); ++l) {
            lines[l].cross = sizes[l];
            lines[l].crossPosition = offsets[l];
        }

        // Places the items along each line, then across it
        for (const Line& line : lines) {
            sizes.clear();
            for (size_t i = 0; i < line.count; ++i) sizes.push_back(items[line.first + i].main);
            flexItems(items, line.first, line.count, availableMain - line.main, sizes);
            distribute(mainFlex, mainAlign, availableMain, gap, sizes, offsets);

            for (size_t i = 0; i < line.count; ++i) {
                Item& item = items[line.first + i];
                item.main = sizes[i];
                item.mainPosition = offsets[i];

                // A Flex's ItemLineAlignment wins over the layout's
                ItemLineAlignment alignment = itemLineAlignment;
                if (item.control->flex && item.control->flex->itemLineAlignment) alignment = *item.control->flex->itemLineAlignment;

                if (alignment == ItemLineAlignment::Stretch) {
                    item.cross = line.cross;
                    item.crossPosition = line.crossPosition;
                } else {
                    Align itemAlign = crossAlign;
                    if (alignment == ItemLineAlignment::Start) itemAlign = Align::Start;
                    else if (alignment == ItemLineAlignment::Center) itemAlign = Align::Center;
                    else if (alignment == ItemLineAlignment::End) itemAlign = Align::End;
                    item.crossPosition = wraps ? line.crossPosition + alignOffset(itemAlign, line.cross, item.cross)
                        : crossOffset(itemAlign, availableCross, item.cross);
                }
            }
        }

        for (const Item& item : items) {
            ControlObject& control = *item.control;
            control.layoutLeft = horizontal ? item.mainPosition : item.crossPosition;
            control.layoutTop = horizontal ? item.crossPosition : item.mainPosition;
            control.layoutWidth = horizontal ? item.main : item.cross;
            control.layoutHeight = horizontal ? item.cross : item.main;
        }
    }

    // Luau. Changes take effect on the next paint, which runs the layout again.

    // Enums are passed as their names. Each table lists the names in the enum's order.
    static const char* const fillDirectionNames[] = { "Vertical", "Horizontal", nullptr };
    static const char* const horizontalAlignmentNames[] = { "Left", "Center", "Right", nullptr };
    static const char* const verticalAlignmentNames[] = { "Top", "Center", "Bottom", nullptr };
    static const char* const sortOrderNames[] = { "LayoutOrder", "Name", nullptr };
    static const char* const flexNames[] = { "None", "Fill", "SpaceAround", "SpaceBetween", "SpaceEvenly", nullptr };

    void ListLayout::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        ControlObject::readDescriptor(L);
        readEnum(L, "FillDirection", fillDirection, fillDirectionNames);
        readEnum(L, "HorizontalAlignment", horizontalAlignment, horizontalAlignmentNames);
        readEnum(L, "VerticalAlignment", verticalAlignment, verticalAlignmentNames);
        readEnum(L, "SortOrder", sortOrder, sortOrderNames);
        readEnum(L, "HorizontalFlex", horizontalFlex, flexNames);
        readEnum(L, "VerticalFlex", verticalFlex, flexNames);
        readEnum(L, "ItemLineAlignment", itemLineAlignment, ItemLineAlignmentNames);
        if (field(L, "Padding")) {
            padding = Luwow::UDims::checkUDimLike(L, -1);
            lua_pop(L, 1);
        }
        readBool(L, "Wraps", wraps);
    }

#define LAYOUT_ENUM_PROPERTY(Property, member, Type, names)                     \
    static int get##Property(lua_State* L, ListLayout& object) {                \
        ControlLuau::pushEnum(L, object.member, names);                         \
        return 1;                                                               \
    }                                                                           \
    static int set##Property(lua_State* L, ListLayout& object) {                \
        object.member = ControlLuau::checkEnum<Type>(L, 3, names);              \
        object.invalidate();                                                    \
        return 0;                                                               \
    }

    LAYOUT_ENUM_PROPERTY(FillDirection, fillDirection, FillDirection, fillDirectionNames)
    LAYOUT_ENUM_PROPERTY(HorizontalAlignment, horizontalAlignment, HorizontalAlignment, horizontalAlignmentNames)
    LAYOUT_ENUM_PROPERTY(VerticalAlignment, verticalAlignment, VerticalAlignment, verticalAlignmentNames)
    LAYOUT_ENUM_PROPERTY(SortOrder, sortOrder, SortOrder, sortOrderNames)
    LAYOUT_ENUM_PROPERTY(HorizontalFlex, horizontalFlex, FlexAlignment, flexNames)
    LAYOUT_ENUM_PROPERTY(VerticalFlex, verticalFlex, FlexAlignment, flexNames)
    LAYOUT_ENUM_PROPERTY(ItemLineAlignment, itemLineAlignment, ItemLineAlignment, ItemLineAlignmentNames)

#undef LAYOUT_ENUM_PROPERTY

    // A UDim, or a number of pixels
    static int getPadding(lua_State* L, ListLayout& object) {
        Luwow::UDims::pushUDim(L, object.padding);
        return 1;
    }

    static int setPadding(lua_State* L, ListLayout& object) {
        object.padding = Luwow::UDims::checkUDimLike(L, 3);
        object.invalidate();
        return 0;
    }

    static int getWraps(lua_State* L, ListLayout& object) {
        lua_pushboolean(L, object.wraps);
        return 1;
    }

    static int setWraps(lua_State* L, ListLayout& object) {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        object.wraps = lua_toboolean(L, 3);
        object.invalidate();
        return 0;
    }

    // The size of the arranged children, the layout runs first so it's current
    static int getAbsoluteContentSize(lua_State* L, ListLayout& object) {
        object.view->update();
        ControlLuau::pushVector2(L, static_cast<float>(object.contentWidth), static_cast<float>(object.contentHeight));
        return 1;
    }

    void ListLayout::RegisterClass(lua_State* L) {
        RegisterGuiObject<ListLayout>(L, {}, {
            { "FillDirection", Bind<getFillDirection>, Bind<setFillDirection> },
            { "HorizontalAlignment", Bind<getHorizontalAlignment>, Bind<setHorizontalAlignment> },
            { "VerticalAlignment", Bind<getVerticalAlignment>, Bind<setVerticalAlignment> },
            { "SortOrder", Bind<getSortOrder>, Bind<setSortOrder> },
            { "Padding", Bind<getPadding>, Bind<setPadding> },
            { "Wraps", Bind<getWraps>, Bind<setWraps> },
            { "HorizontalFlex", Bind<getHorizontalFlex>, Bind<setHorizontalFlex> },
            { "VerticalFlex", Bind<getVerticalFlex>, Bind<setVerticalFlex> },
            { "ItemLineAlignment", Bind<getItemLineAlignment>, Bind<setItemLineAlignment> },
            { "AbsoluteContentSize", Bind<getAbsoluteContentSize> },
        });
    }
} // namespace Luwow::Udc
