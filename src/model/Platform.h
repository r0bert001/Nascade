#pragma once

#include <string>
#include <vector>

namespace nascade {

struct Platform {
    std::string id;
    std::string label;
    std::string romPath;
    std::string imgPath;
    std::vector<std::string> extensions;
    std::string configPath;
    bool fromEmus = false;
    int gameCount = 0;
};

}  // namespace nascade
