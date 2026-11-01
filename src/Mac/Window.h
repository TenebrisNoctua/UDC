#pragma once

#include "IUdcModule.h"
#include <memory>
#include <unordered_map>

namespace Luwow::Udc {

class CocoaSurface;

class Window : public IWindow {
public:
    Window(const WindowDescriptor& descriptor, Luwow::Engine::ILuauHost* host);
    // Draws into another library's NSWindow, through a view of its own inside the window's content view
    explicit Window(void* attachedWindow);
    ~Window() override;
    void show() override;
    void close() override;
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
    bool isEnabled() const override { return enabled; }
    void setEnabled(bool enabled) override;
    bool isTopMost() const override { return topMost; }
    void setTopMost(bool topMost) override;
    void getClientSize(int& width, int& height) const override;
    void setClientSize(int width, int height) override;

    // the user asked to close it, returns whether it closes
    bool closeRequested();
    // it's closing
    void destroyed();
    ControlView* getControlView(Luwow::Engine::ILuauHost* luauHost) override;
    bool isAttached() const override { return attached; }
    void detach() override;

    WindowDescriptor getDescriptor() const { return descriptor; }
    void* getNativeContentView() const;

private:
#ifdef __OBJC__
    // Runs work on the main thread
    void dispatch(void (^work)());
#endif
    // A shown modal window's owner takes input again
    void releaseOwner();

    std::string title;
    Window* owner = nullptr;
    bool modal = false;
    bool enabled = true;
    bool topMost = false;
    bool closed = false;

    WindowDescriptor descriptor;
    void* nativeWindow = nullptr;
    void* nativeDelegate = nullptr;
    // The view the controls are drawn in: an own window's content view, or one inside an attached window's
    void* drawnView = nullptr;
    bool attached = false;
    // An attached window's delegate is the other library's, so closing and focus come as notifications
    void* closeObserver = nullptr;
    void* focusObserver = nullptr;

    std::unique_ptr<CocoaSurface> surface;
    std::unique_ptr<ControlView> controlView;
};

} // namespace Luwow::Udc
