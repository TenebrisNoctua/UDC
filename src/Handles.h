#pragma once

#include "IUdcModule.h"
#include "Userdata.h"
#include "Controls/BaseGuiObject.h"

namespace Luwow::Udc {

// A window is the top of its tree of controls: they're its children, and it has no Parent. It's kept
// alive until it's destroyed, since its controls give it as their Parent.
class LuauWindow : public BaseGuiObject, public Luwow::Objects::ObjectUserdata<LuauWindow> {
public:
    static constexpr const char* Name = "Window";
    static constexpr const char* MetatableName = "Udc.Window";
    using Super = BaseGuiObject;

    LuauWindow(IWindow* window, ControlView* view) : window(window), view(view) {}

    // Pushes a new window's object, the owner of its control view
    static LuauWindow* Create(lua_State* L, IWindow* window, ControlView* view, const std::string& name);

    // window:Show()
    int show(lua_State* L);
    // window:Close()
    int close(lua_State* L);
    // window:Hide()
    int hide(lua_State* L);
    // window:Focus()
    int focus(lua_State* L);
    // window:Center()
    int center(lua_State* L);
    // window:SetClipboardText(text)
    int setClipboardText(lua_State* L);

    // Reads the descriptor
    void readDescriptor(lua_State* L, bool centered);

    IWindow* get() const { return window; }
    ControlView* controls() const { return view; }
    static void RegisterClass(lua_State* L);

    // Destroys the controls, then closes the window. An attached window is left open, only
    // UDC stops drawing into it.
    void destroy(lua_State* L) override;

    // BaseGuiObject
    BaseGuiObject* parentObject() const override { return nullptr; }
    std::vector<BaseGuiObject*> childObjects() const override;

private:
    friend LuauWindow* attachedWindow(lua_State* L, int index, bool push);

    IWindow* window;
    ControlView* view;
    // The native window of an attached one, its key among the attached windows
    void* attachedHandle = nullptr;

    // An attached window's native window closed, its object goes with it
    void attachedClosed();

    // Registry references to the owner's object and to the handlers
    int ownerRef = -1;
    int onCloseRequestedRef = -1;
    int onClosedRef = -1;
    int onFocusedRef = -1;

    // Runs a handler through the scheduler, like a control's
    void fire(int ref, const char* event);
    void setOwnerObject(lua_State* L, int index);

    struct Luau {
        static int getTitle(lua_State* L, LuauWindow& object);
        static int setTitle(lua_State* L, LuauWindow& object);
        static int getOwner(lua_State* L, LuauWindow& object);
        static int setOwner(lua_State* L, LuauWindow& object);
        static int getModal(lua_State* L, LuauWindow& object);
        static int setModal(lua_State* L, LuauWindow& object);
        static int getEnabled(lua_State* L, LuauWindow& object);
        static int setEnabled(lua_State* L, LuauWindow& object);
        static int getTopMost(lua_State* L, LuauWindow& object);
        static int setTopMost(lua_State* L, LuauWindow& object);
        static int getPreventClose(lua_State* L, LuauWindow& object);
        static int setPreventClose(lua_State* L, LuauWindow& object);
        static int getClientSize(lua_State* L, LuauWindow& object);
        static int setClientSize(lua_State* L, LuauWindow& object);
        static int getVisible(lua_State* L, LuauWindow& object);
    };
};

// Registers every UDC userdata metatable on the given state. Signals are resumed through host.
void registerHandles(lua_State* L, Luwow::Engine::ILuauHost* host);

// The module that attaches other libraries' windows, and the host their controls run on
void setAttachingModule(IUdcModule* module, Luwow::Engine::ILuauHost* host);

// The UDC window drawing into another library's window at index. With push, the object is left on the stack.
LuauWindow* attachedWindow(lua_State* L, int index, bool push);

} // namespace Luwow::Udc
