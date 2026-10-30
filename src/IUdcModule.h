#pragma once

#include "ILuauModule.h"
#include "Descriptors.h"
#include <functional>
#include <string>
#include <vector>

namespace Luwow::Engine {
class ILuauHost;
}

// Abstract interface for the UDC API consistent across platform implementations
namespace Luwow::Udc {

class ControlView;

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual void show() = 0;
    virtual void close() = 0;
    virtual void hide() = 0;
    // Brings it to the front and gives it the keyboard
    virtual void focus() = 0;
    // Moves it to the middle of its owner, or of the screen without one
    virtual void center() = 0;

    virtual bool isVisible() const = 0;
    virtual std::string getTitle() const = 0;
    virtual void setTitle(const std::string& title) = 0;
    // An owned window stays above its owner and is minimized with it. A modal one also disables its owner
    // while it's shown, the way a dialog does.
    virtual IWindow* getOwner() const = 0;
    virtual void setOwner(IWindow* owner) = 0;
    virtual bool isModal() const = 0;
    virtual void setModal(bool modal) = 0;
    // Whether it takes input
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    // Whether it stays above the windows of other programs
    virtual bool isTopMost() const = 0;
    virtual void setTopMost(bool topMost) = 0;
    // The size of the area inside the frame, where the controls are
    virtual void getClientSize(int& width, int& height) const = 0;
    virtual void setClientSize(int width, int height) = 0;

    // Run on the window's thread: when the user asks to close it, before it closes unless preventClose
    // is set, and once its native window is gone, closed by the user or by code
    std::function<void()> onCloseRequested;
    std::function<void()> onClosed;
    // Run on the window's thread when it becomes the active window
    std::function<void()> onFocused;
    bool preventClose = false;

    // The window's drawn controls, created with the first one
    virtual ControlView* getControlView(Luwow::Engine::ILuauHost* luauHost) = 0;

    // Whether it's another library's native window that UDC only draws into.
    // Its frame, native controls and closing are left to that library.
    virtual bool isAttached() const = 0;
    // Stops drawing into an attached window and giving it input, the native window stays as it is
    virtual void detach() = 0;
};

// Interface for the UDC API consistent across platforms
class IUdcModule : public Luwow::Engine::ILuauModule {
public:
    virtual ~IUdcModule() = default;
    virtual IWindow* createWindow(const WindowDescriptor& descriptor) = 0;
    // Another library's window to draw controls into, by its native handle. Called on the main thread.
    virtual IWindow* attachWindow(void* nativeHandle) = 0;
};
}