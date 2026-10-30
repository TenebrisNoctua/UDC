#pragma once

#include "Controls/Input.h"

#import <Cocoa/Cocoa.h>

// Pointer input on macOS. Macs don't have touch functionality, 
// except for the Max, which is ignored intentionally because I cannot be bothered to support it.
namespace Luwow::Udc::MacInput {
    // The kind of pointer a mouse event came from
    PointerKind classifyPointer(NSEvent* event);

    // A PointerInput for a mouse event, at a point in the view
    PointerInput pointerInput(NSEvent* event, NSPoint point, PointerButton button);
} // namespace Luwow::Udc::MacInput
