#pragma once

#include <string>
#include <vector>

namespace nascade {

struct Paths {
    std::string sdRoot;
    std::string dataRoot;
    std::string configFile;
    std::string cacheDir;
    std::string mediaCacheDir;
    std::string logsDir;
    std::string emusDir;
    std::string romsDir;
    std::string imgsDir;
    std::string appAssetsDir;

    static Paths detect();
    void ensureDirs() const;
    bool isDevice() const;
    std::vector<std::string> fontCandidates() const;
};

}  // namespace nascade
