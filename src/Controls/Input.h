#pragma once

#include "Canvas.h"

#include <vector>

// Pointer input, shared by every platform.
namespace Luwow::Udc {
    // What a pointer is. Each platform tells them apart its own way.
    enum class PointerKind { Mouse, Touch, Pen };

    // The button of a press.
    enum class PointerButton { None, Primary, Secondary, Middle };

    // A pointer event, in device pixels while the platform sends it
    struct PointerInput {
        int x = 0;
        int y = 0;
        PointerKind kind = PointerKind::Mouse;
        PointerButton button = PointerButton::None;
        KeyModifiers modifiers;
    };

    // The input hardware the system has.
    struct InputCapabilities {
        bool mouse = true;
        bool touch = false;
        bool pen = false;

        bool has(PointerKind kind) const;
    };

    // What the platform has, worked out once
    const InputCapabilities& inputCapabilities();

    // An input that presses a control
    struct PressBinding {
        PointerKind kind;
        PointerButton button;
    };

    // Which inputs fire the pointer events of controls:
    //   OnPressed / OnUnPressed  the inputs in press, when they go down on a control and come back up
    //   OnEnter / OnLeft         the pointer kinds in hover, as they move over controls. Other kinds,
    //                            such as touch, enter a control when they press it and leave when they lift.
    struct InputBindings {
        std::vector<PressBinding> press;
        std::vector<PointerKind> hover;

        bool presses(const PointerInput& input) const;
        bool hovers(PointerKind kind) const;

        // The primary button of a mouse, a touch and a pen tip press. Mice and pens hover. Kinds the
        // system doesn't have are left out.
        static InputBindings defaults(const InputCapabilities& capabilities);
    };

    // The name scripts get for a pointer kind: "Mouse", "Touch" or "Pen"
    const char* pointerKindName(PointerKind kind);
} // namespace Luwow::Udc
