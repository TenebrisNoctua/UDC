#include "Dpi.h"

namespace Luwow::Udc::Dpi {
    namespace {
        // Not every SDK declares these, their values are fixed by Windows
        using DpiContext = HANDLE;
        const DpiContext PerMonitorAwareV2 = reinterpret_cast<DpiContext>(static_cast<INT_PTR>(-4));
        constexpr int ProcessPerMonitorAware = 2;

        template <typename T>
        T lookup(const wchar_t* module, const char* name) {
            HMODULE handle = GetModuleHandleW(module);
            if (!handle) handle = LoadLibraryW(module);
            return handle ? reinterpret_cast<T>(GetProcAddress(handle, name)) : nullptr;
        }
    }

    // Tries Windows 10's per-monitor v2, then Windows 8.1's per-monitor, then Vista's system awareness
    void enableAwareness() {
        static bool done = false;
        if (done) return;
        done = true;

        using SetContext = BOOL(WINAPI*)(DpiContext);
        if (auto setContext = lookup<SetContext>(L"user32.dll", "SetProcessDpiAwarenessContext")) {
            if (setContext(PerMonitorAwareV2) || GetLastError() == ERROR_ACCESS_DENIED) return;
        }

        using SetAwareness = HRESULT(WINAPI*)(int);
        if (auto setAwareness = lookup<SetAwareness>(L"shcore.dll", "SetProcessDpiAwareness")) {
            HRESULT result = setAwareness(ProcessPerMonitorAware);
            if (SUCCEEDED(result) || result == E_ACCESSDENIED) return;
        }

        using SetAware = BOOL(WINAPI*)();
        if (auto setAware = lookup<SetAware>(L"user32.dll", "SetProcessDPIAware")) setAware();
    }

    UINT forWindow(HWND hwnd) {
        using GetForWindow = UINT(WINAPI*)(HWND);
        static auto getForWindow = lookup<GetForWindow>(L"user32.dll", "GetDpiForWindow");
        if (getForWindow && hwnd) {
            if (UINT dpi = getForWindow(hwnd)) return dpi;
        }

        HDC dc = GetDC(hwnd);
        int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 0;
        if (dc) ReleaseDC(hwnd, dc);
        return dpi > 0 ? static_cast<UINT>(dpi) : Default;
    }

    void adjustWindowRect(RECT& rect, HWND hwnd, UINT dpi) {
        DWORD style = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_STYLE));
        DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        BOOL menu = GetMenu(hwnd) != NULL;
        using AdjustForDpi = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        static auto adjustForDpi = lookup<AdjustForDpi>(L"user32.dll", "AdjustWindowRectExForDpi");
        if (adjustForDpi && adjustForDpi(&rect, style, menu, exStyle, dpi)) return;
        AdjustWindowRectEx(&rect, style, menu, exStyle);
    }

    int scale(int value, UINT dpi) {
        return MulDiv(value, static_cast<int>(dpi), static_cast<int>(Default));
    }
} // namespace Luwow::Udc::Dpi
