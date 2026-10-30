#include "Corner.h"

#include <algorithm>
#include <cmath>

namespace Luwow::Udc {
    CornerRadii Corner::resolve(const Rect& frame) const {
        float shorter = static_cast<float>(std::min(frame.width, frame.height));
        auto pixels = [shorter](const UDim& radius) {
            return std::max(0, static_cast<int>(std::lround(radius.resolve(shorter))));
        };
        return CornerRadii(pixels(topLeft), pixels(topRight), pixels(bottomRight), pixels(bottomLeft)).fitted(frame);
    }

    // Luau

    // CornerRadius first, so the corners given on their own win over it
    void Corner::readDescriptor(lua_State* L) {
        ControlObject::readDescriptor(L);
        if (ControlLuau::field(L, "CornerRadius")) {
            topLeft = topRight = bottomRight = bottomLeft = Luwow::UDims::checkUDimLike(L, -1);
            lua_pop(L, 1);
        }
        const std::pair<const char*, UDim*> corners[] = {
            { "TopLeftRadius", &topLeft }, { "TopRightRadius", &topRight },
            { "BottomRightRadius", &bottomRight }, { "BottomLeftRadius", &bottomLeft },
        };
        for (const auto& [name, corner] : corners) {
            if (!ControlLuau::field(L, name)) continue;
            *corner = Luwow::UDims::checkUDimLike(L, -1);
            lua_pop(L, 1);
        }
    }

    // Sets every corner. It reads back as the top left one, the others may differ.
    static int getCornerRadius(lua_State* L, Corner& object) {
        Luwow::UDims::pushUDim(L, object.topLeft);
        return 1;
    }

    static int setCornerRadius(lua_State* L, Corner& object) {
        object.topLeft = object.topRight = object.bottomRight = object.bottomLeft = Luwow::UDims::checkUDimLike(L, 3);
        object.invalidate();
        return 0;
    }

    // Each corner is a UDim, or a number of pixels
#define CORNER_RADIUS(Property, member)                                 \
    static int get##Property(lua_State* L, Corner& object) {            \
        Luwow::UDims::pushUDim(L, object.member);                       \
        return 1;                                                       \
    }                                                                   \
    static int set##Property(lua_State* L, Corner& object) {            \
        object.member = Luwow::UDims::checkUDimLike(L, 3);              \
        object.invalidate();                                            \
        return 0;                                                       \
    }

    CORNER_RADIUS(TopLeftRadius, topLeft)
    CORNER_RADIUS(TopRightRadius, topRight)
    CORNER_RADIUS(BottomRightRadius, bottomRight)
    CORNER_RADIUS(BottomLeftRadius, bottomLeft)

#undef CORNER_RADIUS

    void Corner::RegisterClass(lua_State* L) {
        RegisterGuiObject<Corner>(L, {}, {
            { "CornerRadius", Bind<getCornerRadius>, Bind<setCornerRadius> },
            { "TopLeftRadius", Bind<getTopLeftRadius>, Bind<setTopLeftRadius> },
            { "TopRightRadius", Bind<getTopRightRadius>, Bind<setTopRightRadius> },
            { "BottomRightRadius", Bind<getBottomRightRadius>, Bind<setBottomRightRadius> },
            { "BottomLeftRadius", Bind<getBottomLeftRadius>, Bind<setBottomLeftRadius> },
        });
    }
} // namespace Luwow::Udc
