#include "TextButton.h"
#include "ControlView.h"

namespace Luwow::Udc {
    TextButton::TextButton() : TextObject(ControlKind::TextButton) {
        backgroundColor = 0xE1E1E1;
        borderColor = 0xADADAD;
        textAlignment = TextAlignment::Center;
    }

    void TextButton::paint(ICanvas& canvas, const Rect& frame) {
        std::optional<uint32_t> background = backgroundColor;
        if (pressed && hovered) background = pressedColor;
        else if (hovered) background = hoverColor;

        paintBox(canvas, frame, background, borderColor);
        drawLines(canvas, frame, text, textColor);
    }

    void TextButton::paintEmpty(ICanvas& canvas, const Rect& frame) {
        drawLines(canvas, frame, text, textColor);
    }

    void TextButton::onMouseDown(int x, int y) {
        invalidate();
    }

    void TextButton::onMouseUp(int x, int y, bool inside) {
        invalidate();
        if (inside) view->fireEvent(*this, "OnActivated");
    }

    // Luau

    void TextButton::readDescriptor(lua_State* L) {
        using namespace ControlLuau;
        TextObject::readDescriptor(L);
        readColor(L, "HoverColor", hoverColor);
        readColor(L, "PressedColor", pressedColor);
        readHandler(L, "OnActivated", *this);
    }

    static int getHoverColor(lua_State* L, TextButton& object) {
        ControlLuau::pushColor(L, object.hoverColor);
        return 1;
    }

    static int setHoverColor(lua_State* L, TextButton& object) {
        object.hoverColor = ControlLuau::checkColor(L, 3).value_or(0xE5F1FB);
        object.invalidate();
        return 0;
    }

    static int getPressedColor(lua_State* L, TextButton& object) {
        ControlLuau::pushColor(L, object.pressedColor);
        return 1;
    }

    static int setPressedColor(lua_State* L, TextButton& object) {
        object.pressedColor = ControlLuau::checkColor(L, 3).value_or(0xCCE4F7);
        object.invalidate();
        return 0;
    }

    void TextButton::RegisterClass(lua_State* L) {
        RegisterGuiObject<TextButton>(L, {}, ControlLuau::extend<TextButton>(TextObject::properties<TextButton>(), {
            { "HoverColor", Bind<getHoverColor>, Bind<setHoverColor> },
            { "PressedColor", Bind<getPressedColor>, Bind<setPressedColor> },
        }));
    }
} // namespace Luwow::Udc
