#pragma once

#include "ILuauHost.h"

#include <string>

namespace Luwow::Udc {
    // A file of the package being run, by its path in the package. False when nothing is run from a package
    // or it has no such file, and the path is then one on disk.
    inline bool readPackageFile(Luwow::Engine::ILuauHost* host, const std::string& path, std::string& bytes) {
        if (!host || path.empty()) return false;
        Luwow::Engine::Message read;
        read.topic = Luwow::Engine::Topics::PackageRead;
        read.data = path;
        if (!host->request(read) || read.result != 1) return false;
        bytes = std::move(read.data);
        return true;
    }
} // namespace Luwow::Udc
