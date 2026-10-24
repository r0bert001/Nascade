#pragma once

#include <string>

#include "core/Config.h"
#include "model/Game.h"
#include "scrape/ScraperTypes.h"

namespace nascade {

// 从 thumbnails.libretro.com 的 Named_Boxarts 目录抓取封面。
// 该数据源只有图片，不提供简介、开发商、发行日期等文字元数据。
class LibretroScraper {
public:
    LibretroScraper(ScraperConfig config, std::string cacheDir);

    ScraperTestResult test();
    bool fetch(const Game& game, ScrapedGame& out, std::string& error);

private:
    ScraperConfig config_;
    std::string cacheDir_;
};

}  // namespace nascade
