#include "Input.h"

#include <windowsx.h>

namespace Luwow::Udc {
    // A mouse is assumed when Windows reports none, so the window can still be clicked through remote
    // desktop and similar
    const InputCapabilities& inputCapabilities() {
        static const InputCapabilities capabilities = [] {
            InputCapabilities found;
            int digitizer = GetSystemMetrics(SM_DIGITIZER);
            bool ready = (digitizer & NID_READY) != 0;
            found.touch = ready && (digitizer & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH)) != 0;
            found.pen = ready && (digitizer & (NID_INTEGRATED_PEN | NID_EXTERNAL_PEN)) != 0;
            found.mouse = GetSystemMetrics(SM_MOUSEPRESENT) != 0 || (!found.touch && !found.pen);
            return found;
        }();
        return capabilities;
    }
}

namespace Luwow::Udc::WindowsInput {
    // MI_WP_SIGNATURE in the top 24 bits marks touch and pen input, bit 7 tells touch from pen
    static constexpr LPARAM SignatureMask = 0xFFFFFF00;
    static constexpr LPARAM Signature = 0xFF515700;
    static constexpr LPARAM TouchBit = 0x80;

    PointerKind classifyPointer() {
        LPARAM extra = GetMessageExtraInfo();
        if ((extra & SignatureMask) != Signature) return PointerKind::Mouse;
        return (extra & TouchBit) ? PointerKind::Touch : PointerKind::Pen;
    }

    PointerInput pointerInput(LPARAM lParam, WPARAM wParam, PointerButton button) {
        PointerInput input;
        input.x = GET_X_LPARAM(lParam);
        input.y = GET_Y_LPARAM(lParam);
        input.kind = classifyPointer();
        input.button = button;
        input.modifiers = KeyModifiers{ (wParam & MK_SHIFT) != 0, (wParam & MK_CONTROL) != 0 };
        return input;
    }
} // namespace Luwow::Udc::WindowsInput
