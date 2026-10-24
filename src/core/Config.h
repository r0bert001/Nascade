#pragma once

#include <string>

#include "core/Paths.h"

namespace nascade {

struct SmbProfile {
    std::string name = "我的 NAS";
    std::string host;
    int port = 445;
    std::string share;
    std::string root = "/";
    std::string username;
    std::string password;
    std::string domain;
    bool savePassword = true;
};

struct ScraperConfig {
    bool enabled = true;
    bool consent = false;
    std::string provider = "libretro";
    std::string devId;
    std::string devPassword;
    std::string softName = "Nascade";
    std::string ssId;
    std::string ssPassword;
    std::string language = "zh";
    std::string region = "cn";
    std::string baseUrl = "https://thumbnails.libretro.com";
    bool autoScrape = false;
};

struct AppConfig {
    std::string language = "zh";
    double fontScale = 1.0;
    bool gridView = false;
    bool onboardingDone = false;
    SmbProfile smb;
    ScraperConfig scraper;
};

class ConfigStore {
public:
    static AppConfig defaults();
    static AppConfig load(const Paths& paths);
    static bool save(const Paths& paths, const AppConfig& config);
};

}  // namespace nascade
