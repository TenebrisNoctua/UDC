#pragma once

#include "ObjectUserdata.h"
#include "TextObject.h"

namespace Luwow::Udc {
    // One line of text
    class TextLabel : public TextObject, public Luwow::Objects::ObjectUserdata<TextLabel> {
    public:
        static constexpr const char* Name = "TextLabel";
        using Super = TextObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        TextLabel() : TextObject(ControlKind::TextLabel) {}

        void paint(ICanvas& canvas, const Rect& frame) override;
        void paintEmpty(ICanvas& canvas, const Rect& frame) override;
    };
} // namespace Luwow::Udc
