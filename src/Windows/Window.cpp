#include "Window.h"
#include "GdiCanvas.h"
#include "Dpi.h"
#include "Input.h"
#include "Controls/ControlView.h"
#include "PackageFiles.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <windowsx.h>
#include <commctrl.h>
#include "lua.h"
#include "lualib.h"

namespace Luwow::Udc {

// Work another thread posts to the window's
constexpr UINT WM_LUWOW_CALL = WM_APP + 0x4D;

// WS_CLIPCHILDREN keeps drawn controls from painting over native ones.
Window::Window(const WindowDescriptor& descriptor, Luwow::Engine::ILuauHost* host) : descriptor(descriptor), title(descriptor.Title), topMost(descriptor.TopMost) {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    if (!descriptor.Resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    if (!descriptor.Minimizable) style &= ~WS_MINIMIZEBOX;
    DWORD exStyle = WS_EX_CLIENTEDGE;
    if (descriptor.TopMost) exStyle |= WS_EX_TOPMOST;
    hWnd = CreateWindowExA(
        exStyle,
        "LuwowUdcWindow",
        descriptor.Title.c_str(),
        style,
        descriptor.Left,
        descriptor.Top,
        descriptor.Width,
        descriptor.Height,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (hWnd == NULL) {
        throw std::runtime_error("Failed to create window");
    }

    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

    // The descriptor's size is at 96 DPI. The monitor is only known once the window exists, so it's resized now.
    dpi = Dpi::forWindow(hWnd);
    loadIcons(host);
    if (dpi != Dpi::Default) {
        SetWindowPos(hWnd, NULL, 0, 0, Dpi::scale(descriptor.Width, dpi), Dpi::scale(descriptor.Height, dpi),
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (descriptor.ClientWidth > 0 && descriptor.ClientHeight > 0) setClientSize(descriptor.ClientWidth, descriptor.ClientHeight);
    // The ANSI title above can't hold every character
    setTitle(descriptor.Title);
}

// The window stays the other library's.
Window::Window(HWND window) : attached(true), hWnd(window) {
    int length = GetWindowTextLengthW(hWnd);
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (length > 0) GetWindowTextW(hWnd, text.data(), length + 1);
    title = toUtf8(text);
    topMost = (GetWindowLongPtr(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    dpi = Dpi::forWindow(hWnd);

    SetWindowLongPtr(hWnd, GWL_STYLE, GetWindowLongPtr(hWnd, GWL_STYLE) | WS_CLIPCHILDREN);
    SetWindowSubclass(hWnd, attachedProc, 0, reinterpret_cast<DWORD_PTR>(this));
}

LRESULT CALLBACK Window::attachedProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data) {
    Window* window = reinterpret_cast<Window*>(data);
    LRESULT result = 0;
    if (window->handleControlMessage(message, wParam, lParam, result)) return result;

    // The other library still handles its own destruction, after the controls learn the window is gone
    if (message == WM_DESTROY) {
        RemoveWindowSubclass(hWnd, attachedProc, 0);
        window->destroyed();
    }
    return DefSubclassProc(hWnd, message, wParam, lParam);
}

void Window::detach() {
    dispatch([this] {
        if (!attached || !hWnd) return;
        RemoveWindowSubclass(hWnd, attachedProc, 0);
        // Its own procedure paints the area the controls were in
        InvalidateRect(hWnd, NULL, TRUE);
        hWnd = NULL;
    });
}

ControlView* Window::getControlView(Luwow::Engine::ILuauHost* luauHost) {
    if (!controlView) {
        surface = std::make_unique<GdiSurface>(hWnd);
        controlView = std::make_unique<ControlView>(surface.get(), luauHost);
    }
    return controlView.get();
}

static bool isWindowThread(HWND hWnd) {
    return GetWindowThreadProcessId(hWnd, NULL) == GetCurrentThreadId();
}

void Window::dispatch(std::function<void()> work) {
    if (!hWnd) return;
    if (isWindowThread(hWnd)) {
        work();
        return;
    }
    auto* posted = new std::function<void()>(std::move(work));
    if (!PostMessage(hWnd, WM_LUWOW_CALL, 0, reinterpret_cast<LPARAM>(posted))) delete posted;
}

void Window::show() {
    dispatch([this] {
        if (!hWnd) return;
        if (modal && owner && owner->hWnd) EnableWindow(owner->hWnd, FALSE);
        ShowWindow(hWnd, SW_SHOW);
    });
}

void Window::close() {
    dispatch([this] { destroyNative(); });
}

void Window::hide() {
    dispatch([this] {
        if (!hWnd) return;
        releaseOwner();
        ShowWindow(hWnd, SW_HIDE);
    });
}

void Window::focus() {
    dispatch([this] {
        if (!hWnd) return;
        if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE);
        SetForegroundWindow(hWnd);
    });
}

// Over a shown owner, otherwise in the work area of the monitor it's on, kept inside that work area
void Window::center() {
    dispatch([this] {
        if (!hWnd) return;
        HWND around = (owner && owner->hWnd && IsWindowVisible(owner->hWnd) && !IsIconic(owner->hWnd)) ? owner->hWnd : NULL;

        MONITORINFO monitor{ sizeof(MONITORINFO) };
        GetMonitorInfo(MonitorFromWindow(around ? around : hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
        RECT area = monitor.rcWork;
        RECT target = area;
        if (around) GetWindowRect(around, &target);

        RECT bounds;
        GetWindowRect(hWnd, &bounds);
        int width = bounds.right - bounds.left;
        int height = bounds.bottom - bounds.top;
        int left = target.left + ((target.right - target.left) - width) / 2;
        int top = target.top + ((target.bottom - target.top) - height) / 2;
        left = std::max<int>(area.left, std::min<int>(left, area.right - width));
        top = std::max<int>(area.top, std::min<int>(top, area.bottom - height));
        SetWindowPos(hWnd, NULL, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    });
}

bool Window::isVisible() const {
    return hWnd && IsWindowVisible(hWnd);
}

void Window::setTitle(const std::string& newTitle) {
    title = newTitle;
    dispatch([this, newTitle] {
        if (hWnd) SetWindowTextW(hWnd, toWide(newTitle).c_str());
    });
}

void Window::setOwner(IWindow* newOwner) {
    Window* next = static_cast<Window*>(newOwner);
    dispatch([this, next] {
        if (!hWnd) return;
        bool shownModal = modal && IsWindowVisible(hWnd);
        if (shownModal) releaseOwner();
        owner = next;
        SetWindowLongPtr(hWnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(owner ? owner->hWnd : NULL));
        if (shownModal && owner && owner->hWnd) EnableWindow(owner->hWnd, FALSE);
    });
}

void Window::setModal(bool isModal) {
    dispatch([this, isModal] {
        if (!hWnd || modal == isModal) return;
        bool shown = IsWindowVisible(hWnd);
        if (shown && modal) releaseOwner();
        modal = isModal;
        if (shown && modal && owner && owner->hWnd) EnableWindow(owner->hWnd, FALSE);
    });
}

bool Window::isEnabled() const {
    return hWnd && IsWindowEnabled(hWnd);
}

void Window::setEnabled(bool enabled) {
    dispatch([this, enabled] {
        if (hWnd) EnableWindow(hWnd, enabled);
    });
}

void Window::setTopMost(bool isTopMost) {
    topMost = isTopMost;
    dispatch([this, isTopMost] {
        if (hWnd) SetWindowPos(hWnd, isTopMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    });
}

void Window::getClientSize(int& width, int& height) const {
    width = height = 0;
    RECT client;
    if (!hWnd || !GetClientRect(hWnd, &client)) return;
    int scale = static_cast<int>(dpi ? dpi : Dpi::Default);
    width = MulDiv(client.right - client.left, Dpi::Default, scale);
    height = MulDiv(client.bottom - client.top, Dpi::Default, scale);
}

void Window::setClientSize(int width, int height) {
    dispatch([this, width, height] {
        if (!hWnd) return;
        RECT bounds{ 0, 0, Dpi::scale(width, dpi), Dpi::scale(height, dpi) };
        Dpi::adjustWindowRect(bounds, hWnd, dpi);
        SetWindowPos(hWnd, NULL, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    });
}

void Window::releaseOwner() {
    if (!modal || !owner || !owner->hWnd || !IsWindowVisible(hWnd)) return;
    bool hadFocus = GetForegroundWindow() == hWnd;
    EnableWindow(owner->hWnd, TRUE);
    if (hadFocus) SetForegroundWindow(owner->hWnd);
}

void Window::destroyNative() {
    if (!hWnd) return;
    releaseOwner();
    DestroyWindow(hWnd);
}

void Window::closeRequested() {
    if (onCloseRequested) onCloseRequested();
    if (!preventClose) destroyNative();
}

void Window::destroyed() {
    hWnd = NULL;
    if (onClosed) onClosed();
}

Window::~Window() {
    //DestroyWindow(hWnd);
    if (smallIcon) DestroyIcon(smallIcon);
    if (bigIcon) DestroyIcon(bigIcon);
}

void Window::loadIcons(Luwow::Engine::ILuauHost* host) {
    if (descriptor.Icon.empty()) return;
    std::string bytes;
    bool packaged = readPackageFile(host, descriptor.Icon, bytes);
    auto load = [&](int size) { return packaged ? loadIconData(bytes, size) : loadIconFile(descriptor.Icon, size); };
    HICON small = load(Dpi::scale(16, dpi));
    HICON big = load(Dpi::scale(32, dpi));
    if (small) SendMessageW(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
    if (big) SendMessageW(hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    if (smallIcon) DestroyIcon(smallIcon);
    if (bigIcon) DestroyIcon(bigIcon);
    smallIcon = small;
    bigIcon = big;
}

static Key translateKey(WPARAM virtualKey, KeyModifiers modifiers) {
    bool control = modifiers.control;
    switch (virtualKey) {
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_BACK: return Key::Backspace;
        case VK_DELETE: return modifiers.shift && !control ? Key::Cut : Key::Delete;
        case VK_INSERT:
            if (control && !modifiers.shift) return Key::Copy;
            if (modifiers.shift && !control) return Key::Paste;
            return Key::Other;
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case 'A': return control ? Key::SelectAll : Key::Other;
        case 'C': return control ? Key::Copy : Key::Other;
        case 'X': return control ? Key::Cut : Key::Other;
        case 'V': return control ? Key::Paste : Key::Other;
        default: return Key::Other;
    }
}

static PointerButton buttonOf(UINT message) {
    switch (message) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: return PointerButton::Primary;
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: return PointerButton::Secondary;
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: return PointerButton::Middle;
        default: return PointerButton::None;
    }
}

static int wheelPixels(WPARAM wParam, int& remainder) {
    int units = GET_WHEEL_DELTA_WPARAM(wParam) * 48 + remainder;
    int pixels = units / WHEEL_DELTA;
    remainder = units - pixels * WHEEL_DELTA;
    return pixels;
}

bool Window::handleControlMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    result = 0;

    if (message == WM_LUWOW_CALL) {
        auto* work = reinterpret_cast<std::function<void()>*>(lParam);
        (*work)();
        delete work;
        return true;
    }

    // Moving to a monitor with another scale, or changing the scale.
    if (message == WM_DPICHANGED) {
        dpi = HIWORD(wParam);
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hWnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
            suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (surface) surface->invalidate();
        return true;
    }

    if (!controlView) return false;

    switch (message) {
        case WM_ERASEBKGND:
            // The paint fills the background itself
            result = 1;
            return true;

        case WM_LUWOW_SETTLE:
            surface->settled();
            controlView->settle();
            return true;

        case WM_PAINT:
            surface->paint(*controlView);
            return true;

        case WM_SIZE:
            surface->invalidate();
            return false;

        case WM_MOUSEMOVE: {
            if (!trackingMouse) {
                TRACKMOUSEEVENT track{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                trackingMouse = TrackMouseEvent(&track);
            }
            controlView->pointerMove(WindowsInput::pointerInput(lParam, wParam, PointerButton::None));
            return true;
        }

        case WM_MOUSELEAVE:
            trackingMouse = false;
            controlView->pointerLeave();
            return true;

        // Every button reaches the view, the bindings decide which ones press controls
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            // Takes the keyboard back from a native control that had it
            SetFocus(hWnd);
            controlView->pointerDown(WindowsInput::pointerInput(lParam, wParam, buttonOf(message)));
            return true;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            controlView->pointerUp(WindowsInput::pointerInput(lParam, wParam, buttonOf(message)));
            return true;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            // Wheel positions are in screen coordinates
            POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &point);

            int dx = 0;
            int dy = 0;
            if (message == WM_MOUSEHWHEEL) {
                dx = -wheelPixels(wParam, wheelRemainderX);
            } else if (GetKeyState(VK_SHIFT) & 0x8000) {
                dx = wheelPixels(wParam, wheelRemainderX);
            } else {
                dy = wheelPixels(wParam, wheelRemainderY);
            }
            if (dx == 0 && dy == 0) return true;
            controlView->wheel(point.x, point.y, dx, dy);
            return true;
        }

        case WM_KEYDOWN: {
            KeyModifiers modifiers;
            modifiers.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            modifiers.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            Key key = translateKey(wParam, modifiers);
            if (key == Key::Other) return false;
            controlView->key(key, modifiers);
            return true;
        }

        // Sent by other programs and accessibility tools, handled like the keyboard shortcuts
        case WM_COPY:
            controlView->key(Key::Copy, {});
            return true;
        case WM_CUT:
            controlView->key(Key::Cut, {});
            return true;
        case WM_PASTE:
            controlView->key(Key::Paste, {});
            return true;

        case WM_CHAR: {
            // Characters outside the BMP arrive as two surrogate halves
            wchar_t c = static_cast<wchar_t>(wParam);
            if (c >= 0xD800 && c <= 0xDBFF) {
                pendingSurrogate = c;
                return true;
            }

            std::wstring wide;
            if (c >= 0xDC00 && c <= 0xDFFF) {
                if (!pendingSurrogate) return true;
                wide = { pendingSurrogate, c };
            } else if (c >= 0x20 && c != 0x7F) {
                wide = { c };
            }
            pendingSurrogate = 0;

            if (!wide.empty()) controlView->text(toUtf8(wide));
            return true;
        }

        case WM_SETFOCUS:
            controlView->setWindowFocused(true);
            return false;

        case WM_ACTIVATE:
            if (LOWORD(wParam) != WA_INACTIVE && onFocused) onFocused();
            return false;

        case WM_KILLFOCUS:
            controlView->setWindowFocused(false);
            return false;
    }

    return false;
}

} // namespace Luwow::Udc
