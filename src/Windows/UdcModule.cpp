#include "UdcModule.h"
#include "ILuauHost.h"
#include "Window.h"
#include "Dpi.h"
#include "Handles.h"
#include "Controls/ControlRegistry.h"
#include "GdiCanvas.h"

#include "lua.h"
#include "lualib.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

namespace Luwow::Udc {
using ILuauModule = Luwow::Engine::ILuauModule;
using ILuauHost = Luwow::Engine::ILuauHost;

// For all methods that require the Gui instance, we need to get it from the userdata.
static UdcModule* getModuleInstance(lua_State* L) {
    UdcModule* gui = static_cast<UdcModule*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!gui) {
        throw std::runtime_error("Gui module not found");
    }
    return gui;
}

// Gets the module for a Luau call, which must come from the main thread
static UdcModule* getMainThreadInstance(lua_State* L) {
    UdcModule* gui = getModuleInstance(L);
    if (!gui->isMainThread()) {
        luaL_error(L, "udc can only be used from the main thread");
    }
    return gui;
}

UdcModule::UdcModule() : host(nullptr) {}

ILuauModule* UdcModule::initialize(ILuauHost* host) {
    UdcModule* udc = new UdcModule();
    udc->setHost(host);
    registerHandles(host->getMainState(), host);
    setAttachingModule(udc, host);
    return udc;
}

// Windows not destroyed yet, the message loop ends with the last one
static int openWindows = 0;

// The main thread's hidden window, which other threads post their requests to
static HWND dispatcherWindow = NULL;
static LRESULT CALLBACK DispatcherProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static Window* GetWindow(HWND hWnd) {
    return (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Drawn controls get their messages first
    if (Window* window = GetWindow(hWnd)) {
        LRESULT result = 0;
        if (window->handleControlMessage(message, wParam, lParam, result)) return result;
    }

    switch (message) {
        // The window decides whether it closes
        case WM_CLOSE:
        {
            if (Window* gui = GetWindow(hWnd)) {
                gui->closeRequested();
                return 0;
            }
            break;
        }
        case WM_DESTROY:
        {
            // If all windows opened are closed
            Window* gui = GetWindow(hWnd);
            if (!gui) {
                throw std::runtime_error("Window not found");
            }
            gui->destroyed();
            --openWindows;
            break;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


// Whether a visible window of this thread that isn't UDC's own is open, such as the gui library's
static bool otherWindowsOpen() {
    bool found = false;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND hWnd, LPARAM data) -> BOOL {
        // By class, since other libraries keep their own objects in GWLP_USERDATA too
        char className[32] = {};
        GetClassNameA(hWnd, className, sizeof(className));
        if (!IsWindowVisible(hWnd) || strcmp(className, "LuwowUdcWindow") == 0) return TRUE;
        *reinterpret_cast<bool*>(data) = true;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&found));
    return found;
}

void UdcModule::run() {
    if (!createdWindow) return; // Without a window there's nothing to wait for, and the loop would never end
    MSG msg = {};
    while (openWindows > 0 || otherWindowsOpen()) {
        BOOL result = GetMessage(&msg, NULL, 0, 0);
        if (result == -1) break;
        if (result == 0) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Luwow::Engine::Message exit;
    exit.topic = Luwow::Engine::Topics::HostExit;
    host->request(exit);
}

Luwow::Engine::RunMode UdcModule::getRunMode() const {
    return LUWOW_MODULE_RUN_MODE;
}

void UdcModule::setHost(ILuauHost* host) {
    this->host = host;
    mainThread = std::this_thread::get_id();
    Dpi::enableAwareness();

    WNDCLASSEXA wcex;
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = GetModuleHandle(NULL);
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "LuwowUdcWindow";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExA(&wcex);

    // A message-only window, for the work other threads ask the main thread for
    WNDCLASSEXA dispatcher = { sizeof(WNDCLASSEXA) };
    dispatcher.lpfnWndProc = DispatcherProc;
    dispatcher.hInstance = GetModuleHandle(NULL);
    dispatcher.lpszClassName = "LuwowUdcDispatcher";
    RegisterClassExA(&dispatcher);
    if (!dispatcherWindow) {
        dispatcherWindow = CreateWindowExA(0, "LuwowUdcDispatcher", "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
    }
}

IWindow* UdcModule::createWindow(const WindowDescriptor& descriptor) {
    createdWindow = true;
    ++openWindows;
    return new Window(descriptor);
}

// Not counted among the open windows: the other library's message loop is the one waiting for it
IWindow* UdcModule::attachWindow(void* nativeHandle) {
    HWND hWnd = static_cast<HWND>(nativeHandle);
    if (!IsWindow(hWnd)) throw std::runtime_error("the native window no longer exists");
    // A window's messages are handled on the thread that made it, so its subclass is installed there
    if (GetWindowThreadProcessId(hWnd, NULL) != GetCurrentThreadId()) throw std::runtime_error("the native window belongs to another thread");
    return new Window(hWnd);
}

// Creates the window on the main thread, the one its messages are handled on
static int createWindowNow(lua_State* L) {
    UdcModule* gui = getMainThreadInstance(L);
    WindowDescriptor windowDescriptor = getWindowDescriptor(L);
    IWindow* window = gui->createWindow(windowDescriptor);
    ControlView* view = window->getControlView(gui->getHost());

    LuauWindow* object = LuauWindow::Create(L, window, view, windowDescriptor.Name);
    object->readDescriptor(L, windowDescriptor.Centered);
    return 1;
}

// A window asked for by a coroutine on another thread. 
// The main thread's message loop creates it, then the coroutine is resumed with it.
constexpr UINT WM_LUWOW_CREATE_WINDOW = WM_APP + 0x4E;

struct WindowRequest {
    UdcModule* gui;
    lua_State* thread;
    int threadRef;
    int descriptorRef;
};

static void createRequestedWindow(WindowRequest* request) {
    ILuauHost* host = request->gui->getHost();
    Luwow::Engine::StateLock lock(host);
    lua_State* L = host->getMainState();

    // createWindowNow on a stack of its own, the descriptor at index 1
    lua_State* worker = lua_newthread(L);
    lua_pushlightuserdata(worker, static_cast<ILuauModule*>(request->gui));
    lua_pushcclosure(worker, createWindowNow, "createWindow", 1);
    lua_getref(worker, request->descriptorRef);
    if (lua_pcall(worker, 1, 1, 0) != LUA_OK) {
        const char* error = lua_tostring(worker, -1);
        fprintf(stderr, "[UDC] Error in createWindow: %s\n", error ? error : "unknown error");
        lua_pop(worker, 1);
        lua_pushnil(worker);
    }

    lua_State* thread = request->thread;
    lua_xmove(worker, thread, 1);
    lua_unref(L, request->descriptorRef);
    lua_pop(L, 1);

    Luwow::Engine::Message resume;
    resume.topic = Luwow::Engine::Topics::SchedulerResume;
    resume.state = thread;
    resume.result = 1;
    if (!host->request(resume)) {
        int status = lua_resume(thread, L, 1);
        if (status != LUA_OK && status != LUA_YIELD) {
            const char* error = lua_tostring(thread, -1);
            fprintf(stderr, "[UDC] Error after createWindow: %s\n", error ? error : "unknown error");
        }
    }

    lua_unref(L, request->threadRef);
}

static LRESULT CALLBACK DispatcherProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_LUWOW_CREATE_WINDOW) {
        auto* request = reinterpret_cast<WindowRequest*>(lParam);
        createRequestedWindow(request);
        delete request;
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// udc.createWindow(descriptor)
static int createWindow(lua_State* L) {
    UdcModule* gui = getModuleInstance(L);
    if (gui->isMainThread()) return createWindowNow(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    if (!lua_isyieldable(L) || !dispatcherWindow) {
        luaL_error(L, "udc.createWindow can only be used from the main thread, or a coroutine that can yield");
    }

    auto* request = new WindowRequest{ gui, L, LUA_NOREF, LUA_NOREF };
    lua_pushthread(L);
    request->threadRef = lua_ref(L, -1);
    lua_pop(L, 1);
    lua_pushvalue(L, 1);
    request->descriptorRef = lua_ref(L, -1);
    lua_pop(L, 1);

    if (!PostMessage(dispatcherWindow, WM_LUWOW_CREATE_WINDOW, 0, reinterpret_cast<LPARAM>(request))) {
        lua_unref(L, request->threadRef);
        lua_unref(L, request->descriptorRef);
        delete request;
        luaL_error(L, "couldn't ask the main thread for a window");
    }
    return lua_yield(L, 0);
}

// udc.attach(window)
static int attach(lua_State* L) {
    getMainThreadInstance(L);
    if (!attachedWindow(L, 1, true)) luaL_typeerror(L, 1, "Window of another library, such as gui's");
    return 1;
}

// udc.loadFont(path)
static int loadFont(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    if (!GdiSurface::loadFontFile(path)) luaL_error(L, "couldn't load the font file %s", path);
    return 0;
}

// udc.beep(kind)
static int beep(lua_State* L) {
    std::string kind = luaL_optstring(L, 1, "Default");
    UINT type = MB_OK;
    if (kind == "Information") type = MB_ICONASTERISK;
    else if (kind == "Warning") type = MB_ICONEXCLAMATION;
    else if (kind == "Error") type = MB_ICONHAND;
    else if (kind != "Default") luaL_error(L, "unknown sound %s", kind.c_str());
    MessageBeep(type);
    return 0;
}

const char* UdcModule::getModuleName() const {
    return "udc";
}

const char* UdcModule::getModuleAlias() const {
    return "Luwow";
}

static LuauExport exports[] = {
    { "createWindow", createWindow },
    { "attach", attach },
    { "createView", Udc::createView },
    { "createScrollView", Udc::createScrollView },
    { "createTextLabel", Udc::createTextLabel },
    { "createTextButton", Udc::createTextButton },
    { "createTextInput", Udc::createTextInput },
    { "createListLayout", Udc::createListLayout },
    { "createCorner", Udc::createCorner },
    { "createImageLabel", Udc::createImageLabel },
    { "createPadding", Udc::createPadding },
    { "createStroke", Udc::createStroke },
    { "createImageButton", Udc::createImageButton },
    { "createGradient", Udc::createGradient },
    { "loadFont", loadFont },
    { "beep", beep },
    { nullptr, nullptr }
};

const LuauExport* UdcModule::getExports() const {
    return exports;
}

} // namespace Luwow::Udc

LUWOW_REGISTER_MODULE(Luwow::Udc::UdcModule)
