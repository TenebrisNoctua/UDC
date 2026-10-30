#include "Padding.h"

#include <algorithm>
#include <cmath>

namespace Luwow::Udc {
    static int toPixels(float value) {
        return static_cast<int>(std::lround(value));
    }

    Rect Padding::inset(const Rect& area) const {
        float width = static_cast<float>(area.width);
        float height = static_cast<float>(area.height);
        int l = toPixels(left.resolve(width));
        int r = toPixels(right.resolve(width));
        int t = toPixels(top.resolve(height));
        int b = toPixels(bottom.resolve(height));
        return Rect{ area.x + l, area.y + t, std::max(0, area.width - l - r), std::max(0, area.height - t - b) };
    }

    void Padding::insets(float width, float height, int& horizontal, int& vertical) const {
        horizontal = toPixels(left.resolve(width)) + toPixels(right.resolve(width));
        vertical = toPixels(top.resolve(height)) + toPixels(bottom.resolve(height));
    }

    // Luau

    void Padding::readDescriptor(lua_State* L) {
        ControlObject::readDescriptor(L);
        const std::pair<const char*, UDim*> sides[] = {
            { "PaddingTop", &top }, { "PaddingBottom", &bottom }, { "PaddingLeft", &left }, { "PaddingRight", &right },
        };
        for (const auto& [name, side] : sides) {
            if (!ControlLuau::field(L, name)) continue;
            *side = Luwow::UDims::checkUDimLike(L, -1);
            lua_pop(L, 1);
        }
    }

    // Each side is a UDim, or a number of pixels
#define PADDING_SIDE(Property, member)                                  \
    static int get##Property(lua_State* L, Padding& object) {           \
        Luwow::UDims::pushUDim(L, object.member);                       \
        return 1;                                                       \
    }                                                                   \
    static int set##Property(lua_State* L, Padding& object) {           \
        object.member = Luwow::UDims::checkUDimLike(L, 3);              \
        object.invalidate();                                            \
        return 0;                                                       \
    }

    PADDING_SIDE(PaddingTop, top)
    PADDING_SIDE(PaddingBottom, bottom)
    PADDING_SIDE(PaddingLeft, left)
    PADDING_SIDE(PaddingRight, right)

#undef PADDING_SIDE

    void Padding::RegisterClass(lua_State* L) {
        RegisterGuiObject<Padding>(L, {}, {
            { "PaddingTop", Bind<getPaddingTop>, Bind<setPaddingTop> },
            { "PaddingBottom", Bind<getPaddingBottom>, Bind<setPaddingBottom> },
            { "PaddingLeft", Bind<getPaddingLeft>, Bind<setPaddingLeft> },
            { "PaddingRight", Bind<getPaddingRight>, Bind<setPaddingRight> },
        });
    }
} // namespace Luwow::Udc
