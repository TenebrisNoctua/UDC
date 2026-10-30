#include "Handles.h"
#include "Controls/ControlRegistry.h"
#include "Controls/ControlView.h"
#include "ILuauHost.h"
#include "lualib.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace Luwow::Udc {

// Attached windows, by their native window. Another library's windows are found again by it, and a
// closed one's handle may be used again by a new window, so it's forgotten as soon as it closes.

static IUdcModule* attachingModule = nullptr;
static Luwow::Engine::ILuauHost* attachingHost = nullptr;
static const char* const AttachedWindows = "udc.attachedWindows";

void setAttachingModule(IUdcModule* module, Luwow::Engine::ILuauHost* host) {
    attachingModule = module;
    attachingHost = host;
}

static void pushAttachedTable(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, AttachedWindows);
    if (lua_istable(L, -1)) return;
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, AttachedWindows);
}

static void forgetAttached(lua_State* L, void* handle) {
    pushAttachedTable(L);
    lua_pushlightuserdata(L, handle);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// Another library's window, an userdata typed Window whose getNativeHandle gives its platform window
static void* nativeHandleOf(lua_State* L, int index) {
    if (lua_type(L, index) != LUA_TUSERDATA || Luwow::Objects::BaseObject::is(L, index)) return nullptr;
    if (!luaL_getmetafield(L, index, "__type")) return nullptr;
    bool window = lua_isstring(L, -1) && std::strcmp(lua_tostring(L, -1), "Window") == 0;
    lua_pop(L, 1);
    if (!window) return nullptr;

    lua_getfield(L, index, "getNativeHandle");
    lua_pushvalue(L, index);
    lua_call(L, 1, 1);
    void* handle = lua_tolightuserdata(L, -1);
    lua_pop(L, 1);
    if (!handle) luaL_error(L, "the Window has no native window, it may have been closed");
    return handle;
}

LuauWindow* attachedWindow(lua_State* L, int index, bool push) {
    index = lua_absindex(L, index);
    void* handle = nativeHandleOf(L, index);
    if (!handle) return nullptr;

    pushAttachedTable(L);
    lua_pushlightuserdata(L, handle);
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        LuauWindow* object = Luwow::Objects::BaseObject::check<LuauWindow>(L, -1, "Window");
        if (push) lua_remove(L, -2);
        else lua_pop(L, 2);
        return object;
    }
    lua_pop(L, 1);

    if (!attachingModule) luaL_error(L, "udc isn't initialized");
    IWindow* window = nullptr;
    std::string problem;
    try {
        window = attachingModule->attachWindow(handle);
    } catch (const std::exception& e) {
        problem = e.what();
    }
    if (!window) luaL_error(L, "couldn't draw into the Window: %s", problem.c_str());

    ControlView* view = window->getControlView(attachingHost);
    LuauWindow* object = LuauWindow::Create(L, window, view, "");
    object->attachedHandle = handle;
    window->onClosed = [object] { object->attachedClosed(); };

    lua_pushlightuserdata(L, handle);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    if (push) lua_remove(L, -2);
    else lua_pop(L, 2);
    return object;
}

LuauWindow* LuauWindow::Create(lua_State* L, IWindow* window, ControlView* view, const std::string& name) {
    LuauWindow* object = Push(L, window, view);
    object->name = name;
    object->retain(L);
    view->owner = object;
    return object;
}

void LuauWindow::destroy(lua_State* L) {
    // Destroying it from a script doesn't fire OnClosed
    window->onCloseRequested = nullptr;
    window->onClosed = nullptr;
    window->onFocused = nullptr;
    for (int* ref : { &ownerRef, &onCloseRequestedRef, &onClosedRef, &onFocusedRef }) {
        if (*ref != LUA_NOREF) lua_unref(L, *ref);
        *ref = LUA_NOREF;
    }

    view->destroyAll(L);
    view->owner = nullptr;
    releasePropertySignals(L);
    if (attachedHandle) {
        forgetAttached(L, attachedHandle);
        attachedHandle = nullptr;
        window->detach();
    } else {
        window->close();
    }
    BaseObject::destroy(L);
    release(L);
}

void LuauWindow::attachedClosed() {
    Luwow::Engine::ILuauHost* host = view->luauHost;
    Luwow::Engine::StateLock lock(host);
    if (!isDestroyed()) destroy(host->getMainState());
}

std::vector<BaseGuiObject*> LuauWindow::childObjects() const {
    const auto& children = view->root().children;
    return std::vector<BaseGuiObject*>(children.begin(), children.end());
}

int LuauWindow::show(lua_State* L) {
    window->show();
    return 0;
}

int LuauWindow::close(lua_State* L) {
    window->close();
    return 0;
}

int LuauWindow::hide(lua_State* L) {
    window->hide();
    return 0;
}

int LuauWindow::focus(lua_State* L) {
    window->focus();
    return 0;
}

int LuauWindow::center(lua_State* L) {
    window->center();
    return 0;
}

int LuauWindow::setClipboardText(lua_State* L) {
    size_t length = 0;
    const char* text = luaL_checklstring(L, 2, &length);
    view->surface->setClipboardText(std::string(text, length));
    return 0;
}

// The handler's thread is resumed by the scheduler, so it can yield; without one it runs right away
void LuauWindow::fire(int ref, const char* event) {
    if (ref == LUA_NOREF) return;
    Luwow::Engine::ILuauHost* host = view->luauHost;
    Luwow::Engine::StateLock lock(host);
    lua_State* L = host->getMainState();

    lua_State* thread = lua_newthread(L);
    lua_getref(L, ref);
    lua_xmove(L, thread, 1);

    Luwow::Engine::Message request;
    request.topic = Luwow::Engine::Topics::SchedulerResume;
    request.state = thread;
    request.result = 0;
    if (!host->request(request)) {
        int status = lua_resume(thread, L, 0);
        if (status != LUA_OK && status != LUA_YIELD) {
            const char* error = lua_tostring(thread, -1);
            fprintf(stderr, "[UDC] Error in %s handler: %s\n", event, error ? error : "unknown error");
        }
    }
    lua_pop(L, 1);
}

// A Window or nil, the owner is kept alive while it's referenced here
void LuauWindow::setOwnerObject(lua_State* L, int index) {
    IWindow* owner = nullptr;
    if (!lua_isnil(L, index)) {
        LuauWindow* object = Luwow::Objects::BaseObject::check<LuauWindow>(L, index, "Window");
        if (object == this) luaL_error(L, "a window can't own itself");
        owner = object->window;
    }
    if (ownerRef != LUA_NOREF) lua_unref(L, ownerRef);
    ownerRef = LUA_NOREF;
    if (owner) {
        lua_pushvalue(L, index);
        ownerRef = lua_ref(L, -1);
        lua_pop(L, 1);
    }
    window->setOwner(owner);
}

static int readFunction(lua_State* L, const char* name) {
    lua_getfield(L, 1, name);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return LUA_NOREF;
    }
    luaL_checktype(L, -1, LUA_TFUNCTION);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    return ref;
}

void LuauWindow::readDescriptor(lua_State* L, bool centered) {
    lua_getfield(L, 1, "Owner");
    int ownerIndex = lua_gettop(L);
    if (!lua_isnil(L, ownerIndex)) setOwnerObject(L, ownerIndex);
    lua_pop(L, 1);

    lua_getfield(L, 1, "Modal");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        window->setModal(lua_toboolean(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "PreventClose");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TBOOLEAN);
        window->preventClose = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    onCloseRequestedRef = readFunction(L, "OnCloseRequested");
    onClosedRef = readFunction(L, "OnClosed");
    window->onCloseRequested = [this] { fire(onCloseRequestedRef, "OnCloseRequested"); };
    window->onClosed = [this] { fire(onClosedRef, "OnClosed"); };
    onFocusedRef = readFunction(L, "OnFocused");
    window->onFocused = [this] { fire(onFocusedRef, "OnFocused"); };

    if (centered) window->center();
}

int LuauWindow::Luau::getTitle(lua_State* L, LuauWindow& object) {
    std::string title = object.window->getTitle();
    lua_pushlstring(L, title.data(), title.size());
    return 1;
}

int LuauWindow::Luau::setTitle(lua_State* L, LuauWindow& object) {
    object.window->setTitle(luaL_checkstring(L, 3));
    return 0;
}

int LuauWindow::Luau::getOwner(lua_State* L, LuauWindow& object) {
    if (object.ownerRef == LUA_NOREF) lua_pushnil(L);
    else lua_getref(L, object.ownerRef);
    return 1;
}

int LuauWindow::Luau::setOwner(lua_State* L, LuauWindow& object) {
    object.setOwnerObject(L, 3);
    return 0;
}

int LuauWindow::Luau::getModal(lua_State* L, LuauWindow& object) {
    lua_pushboolean(L, object.window->isModal());
    return 1;
}

int LuauWindow::Luau::setModal(lua_State* L, LuauWindow& object) {
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    object.window->setModal(lua_toboolean(L, 3));
    return 0;
}

int LuauWindow::Luau::getEnabled(lua_State* L, LuauWindow& object) {
    lua_pushboolean(L, object.window->isEnabled());
    return 1;
}

int LuauWindow::Luau::setEnabled(lua_State* L, LuauWindow& object) {
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    object.window->setEnabled(lua_toboolean(L, 3));
    return 0;
}

int LuauWindow::Luau::getTopMost(lua_State* L, LuauWindow& object) {
    lua_pushboolean(L, object.window->isTopMost());
    return 1;
}

int LuauWindow::Luau::setTopMost(lua_State* L, LuauWindow& object) {
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    object.window->setTopMost(lua_toboolean(L, 3));
    return 0;
}

int LuauWindow::Luau::getPreventClose(lua_State* L, LuauWindow& object) {
    lua_pushboolean(L, object.window->preventClose);
    return 1;
}

int LuauWindow::Luau::setPreventClose(lua_State* L, LuauWindow& object) {
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    object.window->preventClose = lua_toboolean(L, 3);
    return 0;
}

int LuauWindow::Luau::getClientSize(lua_State* L, LuauWindow& object) {
    int width = 0, height = 0;
    object.window->getClientSize(width, height);
    lua_pushvector(L, static_cast<float>(width), static_cast<float>(height), 0);
    return 1;
}

int LuauWindow::Luau::setClientSize(lua_State* L, LuauWindow& object) {
    const float* size = luaL_checkvector(L, 3);
    object.window->setClientSize(static_cast<int>(std::lround(size[0])), static_cast<int>(std::lround(size[1])));
    return 0;
}

int LuauWindow::Luau::getVisible(lua_State* L, LuauWindow& object) {
    lua_pushboolean(L, object.window->isVisible());
    return 1;
}

void LuauWindow::RegisterClass(lua_State* L) {
    RegisterGuiObject<LuauWindow>(L, {
        { "Show",  &LuauWindow::show },
        { "Close", &LuauWindow::close },
        { "Hide", &LuauWindow::hide },
        { "Focus", &LuauWindow::focus },
        { "Center", &LuauWindow::center },
        { "SetClipboardText", &LuauWindow::setClipboardText }
    }, {
        { "Title", Bind<&Luau::getTitle>, Bind<&Luau::setTitle> },
        { "Owner", Bind<&Luau::getOwner>, Bind<&Luau::setOwner> },
        { "Modal", Bind<&Luau::getModal>, Bind<&Luau::setModal> },
        { "Enabled", Bind<&Luau::getEnabled>, Bind<&Luau::setEnabled> },
        { "TopMost", Bind<&Luau::getTopMost>, Bind<&Luau::setTopMost> },
        { "PreventClose", Bind<&Luau::getPreventClose>, Bind<&Luau::setPreventClose> },
        { "ClientSize", Bind<&Luau::getClientSize>, Bind<&Luau::setClientSize> },
        { "Visible", Bind<&Luau::getVisible> }
    });
}

void registerHandles(lua_State* L, Luwow::Engine::ILuauHost* host) {
    LuauWindow::RegisterClass(L);
    registerControlClasses(L, host);
}

} // namespace Luwow::Udc
