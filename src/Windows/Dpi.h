#pragma once

#include <windows.h>

// High-DPI support for Windows, kept in one place.
namespace Luwow::Udc::Dpi {
    // The DPI Windows treats as 100% scale
    constexpr UINT Default = 96;

    // Makes the process per-monitor DPI aware, so windows get real pixels instead of being stretched.
    // Does nothing when the awareness was already set, by a manifest for example.
    void enableAwareness();

    // The DPI of the monitor the window is on, or the system DPI for nullptr
    UINT forWindow(HWND hwnd);

    // A value at 96 DPI, scaled to dpi
    int scale(int value, UINT dpi);

    // Grows a client rectangle into the window rectangle with the window's frame at dpi
    void adjustWindowRect(RECT& rect, HWND hwnd, UINT dpi);

} // namespace Luwow::Udc::Dpi
