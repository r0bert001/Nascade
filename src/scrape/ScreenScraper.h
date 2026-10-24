#pragma once

#include <string>

#include "core/Config.h"
#include "model/Game.h"
#include "scrape/ScraperTypes.h"

namespace nascade {

class ScreenScraper {
public:
    explicit ScreenScraper(ScraperConfig config);

    ScraperTestResult test();
    bool fetch(const Game& game, ScrapedGame& out, std::string& error);

private:
    std::string baseUrl() const;
    std::string authQuery() const;
    ScraperConfig config_;
};

}  // namespace nascade
