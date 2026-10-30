#include "Input.h"

namespace Luwow::Udc {
    // A tablet can be plugged in at any time and macOS doesn't list one, so pens are always allowed
    const InputCapabilities& inputCapabilities() {
        static const InputCapabilities capabilities{ true, false, true };
        return capabilities;
    }
}

namespace Luwow::Udc::MacInput {
    PointerKind classifyPointer(NSEvent* event) {
        return [event subtype] == NSEventSubtypeTabletPoint ? PointerKind::Pen : PointerKind::Mouse;
    }

    PointerInput pointerInput(NSEvent* event, NSPoint point, PointerButton button) {
        PointerInput input;
        input.x = static_cast<int>(point.x);
        input.y = static_cast<int>(point.y);
        input.kind = classifyPointer(event);
        input.button = button;
        input.modifiers.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
        input.modifiers.control = ([event modifierFlags] & NSEventModifierFlagCommand) != 0;
        return input;
    }
} // namespace Luwow::Udc::MacInput
