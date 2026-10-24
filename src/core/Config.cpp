#include "core/Config.h"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "core/Util.h"

namespace nascade {

using json = nlohmann::json;

AppConfig ConfigStore::defaults() {
    return AppConfig{};
}

AppConfig ConfigStore::load(const Paths& paths) {
    AppConfig cfg = defaults();

    std::string text;
    if (!util::readFile(paths.configFile, text) || text.empty()) {
        return cfg;
    }

    try {
        const json j = json::parse(text);
        cfg.language = j.value("language", cfg.language);
        cfg.fontScale = j.value("fontScale", cfg.fontScale);
        cfg.gridView = j.value("gridView", cfg.gridView);
        cfg.onboardingDone = j.value("onboardingDone", cfg.onboardingDone);
        cfg.fontScale = std::max(0.8, std::min(1.5, cfg.fontScale));

        if (j.contains("smb") && j["smb"].is_object()) {
            const auto& s = j["smb"];
            cfg.smb.name = s.value("name", cfg.smb.name);
            cfg.smb.host = s.value("host", cfg.smb.host);
            cfg.smb.port = s.value("port", cfg.smb.port);
            cfg.smb.share = s.value("share", cfg.smb.share);
            cfg.smb.root = s.value("root", cfg.smb.root);
            cfg.smb.username = s.value("username", cfg.smb.username);
            cfg.smb.password = s.value("password", cfg.smb.password);
            cfg.smb.domain = s.value("domain", cfg.smb.domain);
            cfg.smb.savePassword = s.value("savePassword", cfg.smb.savePassword);
        }

        if (j.contains("scraper") && j["scraper"].is_object()) {
            const auto& s = j["scraper"];
            cfg.scraper.enabled = s.value("enabled", cfg.scraper.enabled);
            cfg.scraper.consent = s.value("consent", cfg.scraper.consent);
            cfg.scraper.provider = util::toLower(s.value("provider", cfg.scraper.provider));
            cfg.scraper.devId = s.value("devId", cfg.scraper.devId);
            cfg.scraper.devPassword = s.value("devPassword", cfg.scraper.devPassword);
            cfg.scraper.softName = s.value("softName", cfg.scraper.softName);
            cfg.scraper.ssId = s.value("ssId", cfg.scraper.ssId);
            cfg.scraper.ssPassword = s.value("ssPassword", cfg.scraper.ssPassword);
            cfg.scraper.language = s.value("language", cfg.scraper.language);
            cfg.scraper.region = s.value("region", cfg.scraper.region);
            cfg.scraper.baseUrl = s.value("baseUrl", cfg.scraper.baseUrl);
            cfg.scraper.autoScrape = s.value("autoScrape", cfg.scraper.autoScrape);
        }
    } catch (const std::exception& e) {
        Log::instance().warn(std::string("config parse failed: ") + e.what());
    }
    // 兼容旧配置里的 ScreenScraper 应用标识。
    if (cfg.scraper.softName == "BrickVault") cfg.scraper.softName = "Nascade";
    return cfg;
}

bool ConfigStore::save(const Paths& paths, const AppConfig& config) {
    json j;
    j["version"] = 1;
    j["language"] = config.language;
    j["fontScale"] = config.fontScale;
    j["gridView"] = config.gridView;
    j["onboardingDone"] = config.onboardingDone;

    j["smb"] = {
        {"name", config.smb.name},
        {"host", config.smb.host},
        {"port", config.smb.port},
        {"share", config.smb.share},
        {"root", config.smb.root},
        {"username", config.smb.username},
        {"password", config.smb.savePassword ? config.smb.password : ""},
        {"domain", config.smb.domain},
        {"savePassword", config.smb.savePassword},
    };

    j["scraper"] = {
        {"enabled", config.scraper.enabled},
        {"consent", config.scraper.consent},
        {"provider", config.scraper.provider},
        {"devId", config.scraper.devId},
        {"devPassword", config.scraper.devPassword},
        {"softName", config.scraper.softName},
        {"ssId", config.scraper.ssId},
        {"ssPassword", config.scraper.ssPassword},
        {"language", config.scraper.language},
        {"region", config.scraper.region},
        {"baseUrl", config.scraper.baseUrl},
        {"autoScrape", config.scraper.autoScrape},
    };

    util::ensureDir(util::parentPath(paths.configFile));
    return util::writeFile(paths.configFile, j.dump(2) + "\n");
}

}  // namespace nascade
