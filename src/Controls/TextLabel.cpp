#include "TextLabel.h"

namespace Luwow::Udc {
    void TextLabel::paint(ICanvas& canvas, const Rect& frame) {
        ControlObject::paint(canvas, frame);
        drawLines(canvas, frame, text, textColor);
    }

    void TextLabel::paintEmpty(ICanvas& canvas, const Rect& frame) {
        drawLines(canvas, frame, text, textColor);
    }

    void TextLabel::RegisterClass(lua_State* L) {
        RegisterGuiObject<TextLabel>(L, {}, TextObject::properties<TextLabel>());
    }
} // namespace Luwow::Udc
