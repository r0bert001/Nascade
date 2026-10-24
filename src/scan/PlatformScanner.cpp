#include "scan/PlatformScanner.h"

#include <algorithm>
#include <set>

#include <nlohmann/json.hpp>

#include "core/Log.h"
#include "core/Util.h"
#include "scan/RomFiles.h"

namespace nascade {

namespace {

struct FallbackPlatform {
    const char* id;
    const char* label;
    const char* extensions;
};

const FallbackPlatform kFallback[] = {
    {"FC", "FC (NES)", "nes,fds,zip"},
    {"SFC", "SFC (SNES)", "sfc,smc,zip"},
    {"GB", "GB", "gb,zip"},
    {"GBC", "GBC", "gbc,zip"},
    {"GBA", "GBA", "gba,zip"},
    {"MD", "MD (Genesis)", "md,gen,bin,zip"},
    {"PS", "PS (PlayStation)", "cue,bin,chd,pbp,m3u"},
    {"N64", "N64", "z64,n64,v64,zip"},
    {"PCE", "PCE", "pce,zip"},
    {"ARCADE", "Arcade", "zip,7z"},
    {"PSP", "PSP", "iso,cso,pbp"},
    {"DC", "DC (Dreamcast)", "gdi,cdi,chd"},
};

int platformOrder(const std::string& id) {
    for (int i = 0; i < static_cast<int>(sizeof(kFallback) / sizeof(kFallback[0])); ++i) {
        if (util::toLower(kFallback[i].id) == util::toLower(id)) return i;
    }
    return 1000;
}

}  // namespace

std::vector<std::string> PlatformScanner::parseExtlist(const std::string& value) {
    std::vector<std::string> out;
    for (std::string part : util::splitAny(value, "|,; \t")) {
        part = util::toLower(util::trim(part));
        while (!part.empty() && part.front() == '.') part.erase(part.begin());
        if (!part.empty() && part != "*") out.push_back(part);
    }
    return out;
}

std::vector<std::string> PlatformScanner::fallbackExtensions(const std::string& id) {
    for (const auto& item : kFallback) {
        if (util::toLower(item.id) == util::toLower(id)) {
            return parseExtlist(item.extensions);
        }
    }
    return {"zip"};
}

std::string PlatformScanner::resolveFromEmus(const std::string& emuDir, const std::string& value,
                                             const std::string& fallbackPath) {
    if (util::trim(value).empty()) return util::normalizePath(fallbackPath);
    return util::normalizePath(util::joinPath(emuDir, value));
}

void PlatformScanner::countGames(Platform& platform) {
    const std::set<std::string> extensions(platform.extensions.begin(), platform.extensions.end());
    const auto files = filterDiscCompanions(
        listRomFiles(platform.romPath, extensions, platformAllowsExtensionless(platform.id)));
    platform.gameCount = static_cast<int>(files.size());
}

std::vector<Platform> PlatformScanner::fallbackPlatforms(const Paths& paths) {
    std::vector<Platform> result;
    for (const auto& item : kFallback) {
        Platform p;
        p.id = item.id;
        p.label = item.label;
        p.romPath = util::joinPath(paths.romsDir, item.id);
        p.imgPath = util::joinPath(paths.imgsDir, item.id);
        p.extensions = augmentPlatformExtensions(p.id, parseExtlist(item.extensions));
        p.fromEmus = false;
        countGames(p);
        result.push_back(std::move(p));
    }
    return result;
}

std::vector<Platform> PlatformScanner::scan(const Paths& paths) {
    std::vector<Platform> result;
    std::set<std::string> seen;

    const auto emuEntries = util::listDir(paths.emusDir);
    for (const auto& entry : emuEntries) {
        if (!entry.isDir) continue;
        const std::string lowerTag = util::toLower(entry.name);
        if (lowerTag == "psbios" || lowerTag == "bak" ||
            util::startsWith(lowerTag, "_sample")) {
            continue;
        }
        if (seen.count(lowerTag) > 0) continue;

        const std::string configPath = util::joinPath(util::joinPath(paths.emusDir, entry.name), "config.json");
        std::string text;
        if (!util::readFile(configPath, text)) {
            Log::instance().warn("platform config missing: " + configPath);
            continue;
        }

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(text);
        } catch (const std::exception& e) {
            Log::instance().warn("platform config invalid: " + configPath + " (" + e.what() + ")");
            continue;
        }

        Platform p;
        p.id = entry.name;
        p.label = j.value("label", entry.name);
        const std::string emuDir = util::joinPath(paths.emusDir, entry.name);
        p.romPath = resolveFromEmus(emuDir, j.value("rompath", ""),
                                    util::joinPath(paths.romsDir, entry.name));
        p.imgPath = resolveFromEmus(emuDir, j.value("imgpath", ""),
                                    util::joinPath(paths.imgsDir, entry.name));
        p.extensions = parseExtlist(j.value("extlist", ""));
        if (p.extensions.empty()) p.extensions = fallbackExtensions(p.id);
        p.extensions = augmentPlatformExtensions(p.id, p.extensions);
        p.configPath = configPath;
        p.fromEmus = true;
        countGames(p);
        seen.insert(util::toLower(p.id));
        result.push_back(std::move(p));
    }

    if (result.empty()) {
        Log::instance().warn("no Emus configs found, using fallback platform table");
        result = fallbackPlatforms(paths);
    }

    std::sort(result.begin(), result.end(), [](const Platform& a, const Platform& b) {
        const int oa = platformOrder(a.id);
        const int ob = platformOrder(b.id);
        if (oa != ob) return oa < ob;
        return util::toLower(a.label) < util::toLower(b.label);
    });

    Log::instance().info("platform scan found " + std::to_string(result.size()) + " platforms");
    return result;
}

}  // namespace nascade
