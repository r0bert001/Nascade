#pragma once

#include <memory>
#include <string>

#include "core/Config.h"
#include "vfs/Vfs.h"

namespace nascade {

std::string localNasRootFromEnv();
std::unique_ptr<Vfs> makeVfs(const SmbProfile& profile, const std::string& localNasRoot,
                             std::string& error);

}  // namespace nascade
