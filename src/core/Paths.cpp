#include "core/Paths.h"

#include <cstdio>

#include "core/Util.h"

namespace nascade {

Paths Paths::detect() {
    Paths p;

    std::string sd = util::getEnv("NASCADE_SD_ROOT");
    if (sd.empty()) {
        if (util::isDirectory("/mnt/SDCARD")) {
            sd = "/mnt/SDCARD";
        } else {
            sd = util::joinPath(util::currentWorkingDir(), "runtime");
        }
    }
    p.sdRoot = util::normalizePath(sd);
    p.dataRoot = util::joinPath(p.sdRoot, ".Nascade");
    // 从 BrickVault 升级时迁移旧数据目录，保留 SMB 配置、刮削缓存和日志。
    const std::string legacyRoot = util::joinPath(p.sdRoot, ".BrickVault");
    if (!util::isDirectory(p.dataRoot) && util::isDirectory(legacyRoot)) {
        std::rename(legacyRoot.c_str(), p.dataRoot.c_str());
    }
    p.configFile = util::joinPath(p.dataRoot, "config.json");
    p.cacheDir = util::joinPath(p.dataRoot, "cache");
    p.mediaCacheDir = util::joinPath(p.cacheDir, "media");
    p.logsDir = util::joinPath(p.dataRoot, "logs");
    const std::string legacyLog = util::joinPath(p.logsDir, "BrickVault.log");
    const std::string newLog = util::joinPath(p.logsDir, "Nascade.log");
    if (!util::fileExists(newLog) && util::fileExists(legacyLog)) {
        std::rename(legacyLog.c_str(), newLog.c_str());
    }
    p.emusDir = util::joinPath(p.sdRoot, "Emus");
    p.romsDir = util::joinPath(p.sdRoot, "Roms");
    p.imgsDir = util::joinPath(p.sdRoot, "Imgs");
    p.appAssetsDir = util::joinPath(p.sdRoot, "Apps/Nascade.pak/assets");
    return p;
}

void Paths::ensureDirs() const {
    util::ensureDir(dataRoot);
    util::ensureDir(cacheDir);
    util::ensureDir(mediaCacheDir);
    util::ensureDir(logsDir);
}

bool Paths::isDevice() const {
    return sdRoot == "/mnt/SDCARD";
}

std::vector<std::string> Paths::fontCandidates() const {
    return {
        util::joinPath(appAssetsDir, "fonts/NotoSansSC-Regular.otf"),
        util::joinPath(appAssetsDir, "fonts/NotoSansCJKsc-Regular.otf"),
        util::joinPath(sdRoot, "assets/fonts/NotoSansSC-Regular.otf"),
        util::joinPath(util::currentWorkingDir(), "assets/fonts/NotoSansSC-Regular.otf"),
        util::joinPath(util::currentWorkingDir(), "assets/fonts/NotoSansCJKsc-Regular.otf"),
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
    };
}

}  // namespace nascade
