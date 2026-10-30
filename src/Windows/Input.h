#pragma once

#include "Controls/Input.h"

#include <windows.h>

// Pointer input on Windows.
namespace Luwow::Udc::WindowsInput {
    // The kind of pointer the mouse message being handled came from
    PointerKind classifyPointer();

    // A PointerInput for a mouse message's lParam and wParam
    PointerInput pointerInput(LPARAM lParam, WPARAM wParam, PointerButton button);
} // namespace Luwow::Udc::WindowsInput
