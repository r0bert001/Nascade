#pragma once

#include <string>
#include <vector>

#include "core/Paths.h"
#include "model/Platform.h"

namespace nascade {

class PlatformScanner {
public:
    static std::vector<Platform> scan(const Paths& paths);

private:
    static std::vector<Platform> fallbackPlatforms(const Paths& paths);
    static std::vector<std::string> parseExtlist(const std::string& value);
    static std::vector<std::string> fallbackExtensions(const std::string& id);
    static std::string resolveFromEmus(const std::string& emuDir, const std::string& value,
                                       const std::string& fallbackPath);
    static void countGames(Platform& platform);
};

}  // namespace nascade
