#pragma once

#include "ObjectUserdata.h"
#include "ControlObject.h"

namespace Luwow::Udc {
    // A plain container, with an optional background and border
    class View : public ControlObject, public Luwow::Objects::ObjectUserdata<View> {
    public:
        static constexpr const char* Name = "View";
        using Super = ControlObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        View() : ControlObject(ControlKind::View) {}
    };
} // namespace Luwow::Udc
