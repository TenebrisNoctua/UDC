#include "View.h"

namespace Luwow::Udc {
    void View::RegisterClass(lua_State* L) {
        RegisterGuiObject<View>(L, {}, ControlObject::properties<View>());
    }
} // namespace Luwow::Udc
