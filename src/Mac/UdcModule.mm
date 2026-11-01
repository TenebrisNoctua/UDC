#import <Cocoa/Cocoa.h>

#include "UdcModule.h"
#include "ILuauHost.h"
#include "Window.h"
#include "Handles.h"
#include "Controls/ControlRegistry.h"
#include "CocoaCanvas.h"

#include "lua.h"
#include "lualib.h"
#include <cstdio>
#include <stdexcept>
#include <string>

// Whether the application was asked to quit
static bool quitRequested = false;

@interface LuwowUdcApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation LuwowUdcApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    quitRequested = true;
    [sender stop:nil];

    // stop: takes effect once an event has been handled, this one makes sure there's one
    NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0
        timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [sender postEvent:wake atStart:YES];
    return NSTerminateCancel;
}
@end

// NSApp only keeps a weak reference to its delegate
static LuwowUdcApplicationDelegate* applicationDelegate = nil;

namespace Luwow::Udc {
using ILuauModule = Luwow::Engine::ILuauModule;
using ILuauHost = Luwow::Engine::ILuauHost;

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


// Once the windows are closed the program ends.
void UdcModule::run() {
    if (!createdWindow) return; // Without a window there's nothing to wait for, and the loop would never end
    // Quitting before the loop started, such as by closing every window from the script, skips it
    if (!quitRequested) {
        @autoreleasepool {
            [NSApp activateIgnoringOtherApps:YES];
            [NSApp run];
        }
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

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        applicationDelegate = [[LuwowUdcApplicationDelegate alloc] init];
        [NSApp setDelegate:applicationDelegate];
        [NSApp finishLaunching];
    }
}

IWindow* UdcModule::createWindow(const WindowDescriptor& descriptor) {
    createdWindow = true;
    return new Window(descriptor, host);
}

// Not counted among the open windows, the other library's event loop is the one waiting for it
IWindow* UdcModule::attachWindow(void* nativeHandle) {
    if (![NSThread isMainThread]) throw std::runtime_error("windows are attached on the main thread");
    id window = (__bridge id)nativeHandle;
    if (![window isKindOfClass:[NSWindow class]]) throw std::runtime_error("the native handle isn't an NSWindow");
    return new Window(nativeHandle);
}

// Creates the window on the main thread
static int createWindowNow(lua_State* L) {
    UdcModule* gui = getMainThreadInstance(L);
    WindowDescriptor windowDescriptor = getWindowDescriptor(L);
    IWindow* window = gui->createWindow(windowDescriptor);
    ControlView* view = window->getControlView(gui->getHost());

    LuauWindow* object = LuauWindow::Create(L, window, view, windowDescriptor.Name);
    object->readDescriptor(L, windowDescriptor.Centered);
    return 1;
}

// udc.createWindow(descriptor)
static int createWindow(lua_State* L) {
    UdcModule* gui = getModuleInstance(L);
    if (gui->isMainThread()) return createWindowNow(L);

    luaL_checktype(L, 1, LUA_TTABLE);
    if (!lua_isyieldable(L)) {
        luaL_error(L, "udc.createWindow can only be used from the main thread, or a coroutine that can yield");
    }

    lua_pushthread(L);
    int threadRef = lua_ref(L, -1);
    lua_pop(L, 1);
    lua_pushvalue(L, 1);
    int descriptorRef = lua_ref(L, -1);
    lua_pop(L, 1);
    lua_State* thread = L;

    dispatch_async(dispatch_get_main_queue(), ^{
        ILuauHost* host = gui->getHost();
        Luwow::Engine::StateLock lock(host);
        lua_State* main = host->getMainState();

        lua_State* worker = lua_newthread(main);
        lua_pushlightuserdata(worker, static_cast<Luwow::Engine::ILuauModule*>(gui));
        lua_pushcclosure(worker, createWindowNow, "createWindow", 1);
        lua_getref(worker, descriptorRef);
        if (lua_pcall(worker, 1, 1, 0) != LUA_OK) {
            const char* error = lua_tostring(worker, -1);
            fprintf(stderr, "[UDC] Error in createWindow: %s\n", error ? error : "unknown error");
            lua_pop(worker, 1);
            lua_pushnil(worker);
        }
        lua_xmove(worker, thread, 1);
        lua_unref(main, descriptorRef);
        lua_pop(main, 1);

        Luwow::Engine::Message resume;
        resume.topic = Luwow::Engine::Topics::SchedulerResume;
        resume.state = thread;
        resume.result = 1;
        if (!host->request(resume)) {
            int status = lua_resume(thread, main, 1);
            if (status != LUA_OK && status != LUA_YIELD) {
                const char* error = lua_tostring(thread, -1);
                fprintf(stderr, "[UDC] Error after createWindow: %s\n", error ? error : "unknown error");
            }
        }

        lua_unref(main, threadRef);
    });
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
    if (!CocoaSurface::loadFontFile(path)) luaL_error(L, "couldn't load the font file %s", path);
    return 0;
}

const char* UdcModule::getModuleName() const {
    return "udc";
}

const char* UdcModule::getModuleAlias() const {
    return "Luwow";
}

// udc.beep(kind)
static int beep(lua_State* L) {
    std::string kind = luaL_optstring(L, 1, "Default");
    if (kind != "Information" && kind != "Warning" && kind != "Error" && kind != "Default") {
        luaL_error(L, "unknown sound %s", kind.c_str());
    }
    NSBeep();
    return 0;
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
