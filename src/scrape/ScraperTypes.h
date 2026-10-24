#pragma once

#include <string>

namespace nascade {

struct ScraperTestResult {
    bool ok = false;
    int platformCount = 0;
    std::string message;
};

struct ScrapedGame {
    std::string providerGameId;
    std::string title;
    std::string description;
    std::string genre;
    std::string developer;
    std::string publisher;
    std::string releaseDate;
    std::string players;
    std::string rating;
    std::string coverUrl;
    std::string screenshotUrl;
};

}  // namespace nascade
