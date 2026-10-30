#pragma once

#include "IUdcModule.h"
#include <functional>
#include <memory>
#include <vector>
#include <windows.h>
#include <unordered_map>

namespace Luwow::Udc {

class GdiSurface;

class Window : public IWindow {
public:
    Window(const WindowDescriptor& descriptor);
    // Draws into another library's window, through a subclass of it rather than a window of its own
    explicit Window(HWND attached);
    ~Window();
    virtual void show() override;
    virtual void close() override;
    void hide() override;
    void focus() override;
    void center() override;

    bool isVisible() const override;
    std::string getTitle() const override { return title; }
    void setTitle(const std::string& title) override;
    IWindow* getOwner() const override { return owner; }
    void setOwner(IWindow* owner) override;
    bool isModal() const override { return modal; }
    void setModal(bool modal) override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;
    bool isTopMost() const override { return topMost; }
    void setTopMost(bool topMost) override;
    void getClientSize(int& width, int& height) const override;
    void setClientSize(int width, int height) override;

    // the user asked to close it
    void closeRequested();
    // the native window is gone
    void destroyed();
    ControlView* getControlView(Luwow::Engine::ILuauHost* luauHost) override;
    bool isAttached() const override { return attached; }
    void detach() override;

    // Handles the window messages drawn controls need, returns false to leave a message to the default handling
    bool handleControlMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    // Accessors
    WindowDescriptor getDescriptor() const { return descriptor; };
    HWND getHWND() const { return hWnd; };

private:
    // The subclass procedure of an attached window, its messages before its own procedure has them
    static LRESULT CALLBACK attachedProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);
    bool attached = false;

    int wheelRemainderX = 0;
    int wheelRemainderY = 0;
    // Runs work on the window's thread
    void dispatch(std::function<void()> work);
    // Destroys the native window, giving a modal one's owner its input back first
    void destroyNative();
    // A shown modal window's owner takes input again, and the focus when this one had it
    void releaseOwner();

    std::string title;
    Window* owner = nullptr;
    bool modal = false;
    bool topMost = false;

    WindowDescriptor descriptor;
    HWND hWnd;

    std::unique_ptr<GdiSurface> surface;
    std::unique_ptr<ControlView> controlView;
    bool trackingMouse = false;
    wchar_t pendingSurrogate = 0;

    UINT dpi = 96;

    // The descriptor's icon, at the title bar's and the taskbar's sizes
    HICON smallIcon = nullptr;
    HICON bigIcon = nullptr;
    void loadIcons();
};

} // namespace Luwow::Udc
