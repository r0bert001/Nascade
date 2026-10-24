#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nascade {

struct Game {
    std::string name;
    std::string romName;
    std::string platformId;
    std::string path;
    std::string localPath;
    std::string coverPath;
    std::string description;
    std::string format;
    uint64_t size = 0;
    int64_t mtime = 0;
    bool installed = false;
    bool remote = false;
    std::vector<std::string> files;
};

}  // namespace nascade
