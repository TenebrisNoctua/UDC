#include "Input.h"

#include <algorithm>

namespace Luwow::Udc {
    bool InputCapabilities::has(PointerKind kind) const {
        switch (kind) {
            case PointerKind::Mouse: return mouse;
            case PointerKind::Touch: return touch;
            case PointerKind::Pen: return pen;
        }
        return false;
    }

    bool InputBindings::presses(const PointerInput& input) const {
        return std::any_of(press.begin(), press.end(), [&](const PressBinding& binding) {
            return binding.kind == input.kind && binding.button == input.button;
        });
    }

    bool InputBindings::hovers(PointerKind kind) const {
        return std::find(hover.begin(), hover.end(), kind) != hover.end();
    }

    InputBindings InputBindings::defaults(const InputCapabilities& capabilities) {
        InputBindings bindings;
        for (PressBinding binding : {
            PressBinding{ PointerKind::Mouse, PointerButton::Primary },
            PressBinding{ PointerKind::Touch, PointerButton::Primary },
            PressBinding{ PointerKind::Pen, PointerButton::Primary },
        }) {
            if (capabilities.has(binding.kind)) bindings.press.push_back(binding);
        }
        for (PointerKind kind : { PointerKind::Mouse, PointerKind::Pen }) {
            if (capabilities.has(kind)) bindings.hover.push_back(kind);
        }
        return bindings;
    }

    const char* pointerKindName(PointerKind kind) {
        switch (kind) {
            case PointerKind::Mouse: return "Mouse";
            case PointerKind::Touch: return "Touch";
            case PointerKind::Pen: return "Pen";
        }
        return "Mouse";
    }
} // namespace Luwow::Udc
