#pragma once

#include "ObjectUserdata.h"
#include "TextObject.h"

namespace Luwow::Udc {
    // A button drawn with text, firing OnActivated when released over it
    class TextButton : public TextObject, public Luwow::Objects::ObjectUserdata<TextButton> {
    public:
        static constexpr const char* Name = "TextButton";
        using Super = TextObject;
        // Registers its Luau members, called by ControlRegistry
        static void RegisterClass(lua_State* L);

        TextButton();

        uint32_t hoverColor = 0xE5F1FB;
        uint32_t pressedColor = 0xCCE4F7;

        void paint(ICanvas& canvas, const Rect& frame) override;
        void paintEmpty(ICanvas& canvas, const Rect& frame) override;
        bool activeByDefault() const override { return true; }
        void onMouseDown(int x, int y) override;
        void onMouseUp(int x, int y, bool inside) override;

        // Luau
        void readDescriptor(lua_State* L) override;
    };
} // namespace Luwow::Udc
