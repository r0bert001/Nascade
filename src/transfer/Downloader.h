#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "core/Config.h"

namespace nascade {

struct DownloadResult {
    bool ok = false;
    bool resumed = false;
    uint64_t bytes = 0;
    std::string error;
};

DownloadResult downloadFile(const SmbProfile& profile, const std::string& localNasRoot,
                            const std::string& remotePath, const std::string& localPath,
                            uint64_t expectedSize, std::atomic<uint64_t>* progress);

}  // namespace nascade
