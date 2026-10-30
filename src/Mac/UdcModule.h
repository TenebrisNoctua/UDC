#pragma once
#include "IUdcModule.h"
#include "ILuauHost.h"

#include <thread>

namespace Luwow::Udc {

using ILuauHost = Luwow::Engine::ILuauHost;
using RunMode = Luwow::Engine::RunMode;

class UdcModule : public IUdcModule {
public:
    UdcModule();
    ~UdcModule() = default;

    const char* getModuleName() const override;
    const char* getModuleAlias() const override;
    const LuauExport* getExports() const override;
    ILuauModule* initialize(ILuauHost* host) override;
    IWindow* createWindow(const WindowDescriptor& descriptor) override;
    IWindow* attachWindow(void* nativeHandle) override;

    RunMode getRunMode() const override;
    ILuauHost* getHost() const { return host; }

    // Windows belong to the thread that created them, so the API is limited to the main thread
    bool isMainThread() const { return std::this_thread::get_id() == mainThread; }
    // Runs the window message loop on the main thread until the windows are closed
    void run() override;
private:
    void setHost(ILuauHost* host);
    ILuauHost* host;
    std::thread::id mainThread;
    bool createdWindow = false;
};

} // namespace Luwow::Udc
