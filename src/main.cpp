#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/App.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"
#include "scrape/LibretroScraper.h"
#include "scrape/ScreenScraper.h"
#include "transfer/Downloader.h"
#include "vfs/VfsFactory.h"

#ifdef NASCADE_HAVE_LIBSMB2
#include "vfs/SmbVfs.h"
#endif

namespace {

void printUsage() {
    std::printf(
        "Nascade 0.1.0 (M0)\n"
        "用法: nascade [选项]\n"
        "  --windowed              强制窗口模式（桌面调试）\n"
        "  --screenshot <文件.ppm> 渲染一帧并保存为 PPM 后退出\n"
        "  --screen <名称>         截图页面: home|platforms|games|detail|settings|about|smb|scraper|logs|remote\n"
        "  --test-scraper          使用配置/环境变量测试 ScreenScraper 连接\n"
        "  --test-scrape           使用配置/环境变量测试单游戏元数据解析\n"
        "  --test-smb              使用配置/环境变量测试 SMB 连接并列出根目录\n"
        "  --test-download         下载一个远程文件（配合 NASCADE_LOCAL_NAS 可离线测试）\n"
        "  --test-ui-download      通过应用内部流程测试远程浏览+下载（需设置 NASCADE_REMOTE_DIR / NASCADE_REMOTE_FILE）\n"
        "  --test-ui-scrape        通过应用内部流程测试刮削+封面写入（需 ScreenScraper 凭据）\n"
        "  -h, --help              显示帮助\n"
        "\n"
        "环境变量:\n"
        "  NASCADE_SD_ROOT      覆盖 SD 根目录（桌面调试）\n"
        "  NASCADE_SCRAPER_PROVIDER  libretro|screenscraper\n"
        "  NASCADE_LIBRETRO_BASEURL  默认 https://thumbnails.libretro.com\n"
        "  NASCADE_SS_DEVID / NASCADE_SS_DEVPASSWORD\n"
        "  NASCADE_SS_SSID / NASCADE_SS_SSPASSWORD\n"
        "  NASCADE_SS_BASEURL\n"
        "  NASCADE_SMB_HOST / NASCADE_SMB_SHARE / NASCADE_SMB_ROOT\n"
        "  NASCADE_SMB_USER / NASCADE_SMB_PASSWORD / NASCADE_SMB_DOMAIN\n"
        "  NASCADE_SMB_PORT\n"
        "  NASCADE_LOCAL_NAS    用本地目录模拟 NAS（桌面测试）\n"
        "  NASCADE_REMOTE_PATH / NASCADE_LOCAL_PATH / NASCADE_REMOTE_SIZE\n"
        "  NASCADE_REMOTE_DIR / NASCADE_REMOTE_FILE\n"
        "  NASCADE_SCRAPE_PLATFORM（默认 GBA）\n");
}

void overrideEnv(const char* name, std::string& field) {
    const std::string value = nascade::util::getEnv(name);
    if (!value.empty()) field = value;
}

void applyScraperEnv(nascade::AppConfig& config) {
    overrideEnv("NASCADE_SS_DEVID", config.scraper.devId);
    overrideEnv("NASCADE_SS_DEVPASSWORD", config.scraper.devPassword);
    overrideEnv("NASCADE_SS_SSID", config.scraper.ssId);
    overrideEnv("NASCADE_SS_SSPASSWORD", config.scraper.ssPassword);
    overrideEnv("NASCADE_SS_BASEURL", config.scraper.baseUrl);
    overrideEnv("NASCADE_SCRAPER_PROVIDER", config.scraper.provider);
    overrideEnv("NASCADE_LIBRETRO_BASEURL", config.scraper.baseUrl);
    config.scraper.provider = nascade::util::toLower(config.scraper.provider);
}

int runScraperTest() {
    nascade::Paths paths = nascade::Paths::detect();
    paths.ensureDirs();
    nascade::Log::instance().init(nascade::util::joinPath(paths.logsDir, "Nascade.log"));

    nascade::AppConfig config = nascade::ConfigStore::load(paths);
    applyScraperEnv(config);

    nascade::ScraperTestResult result;
    if (config.scraper.provider == "libretro") {
        nascade::LibretroScraper scraper(config.scraper, paths.cacheDir);
        result = scraper.test();
    } else {
        nascade::ScreenScraper scraper(config.scraper);
        result = scraper.test();
    }
    std::printf("%s\n", result.message.c_str());
    return result.ok ? 0 : 1;
}

int runScrapeTest() {
    nascade::Paths paths = nascade::Paths::detect();
    paths.ensureDirs();
    nascade::Log::instance().init(nascade::util::joinPath(paths.logsDir, "Nascade.log"));

    nascade::AppConfig config = nascade::ConfigStore::load(paths);
    applyScraperEnv(config);

    nascade::Game game;
    const std::string testFile = nascade::util::getEnv("NASCADE_SCRAPE_FILE");
    if (!testFile.empty()) {
        game.path = testFile;
        game.localPath = testFile;
        game.romName = nascade::util::baseNameNoExt(testFile);
        game.name = game.romName;
        game.platformId = nascade::util::getEnv("NASCADE_SCRAPE_PLATFORM", "GBA");
        uint64_t size = 0;
        nascade::util::fileSize(testFile, size);
        game.size = size;
    } else {
        game.name = "Metroid Fusion";
        game.romName = "Metroid Fusion (USA)";
        game.path = "Metroid Fusion (USA).gba";
        game.platformId = "GBA";
        game.size = 8388608;
    }

    nascade::ScrapedGame result;
    std::string error;
    bool ok = false;
    if (config.scraper.provider == "libretro") {
        nascade::LibretroScraper scraper(config.scraper, paths.cacheDir);
        ok = scraper.fetch(game, result, error);
    } else {
        nascade::ScreenScraper scraper(config.scraper);
        ok = scraper.fetch(game, result, error);
    }
    if (!ok) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    std::printf("title: %s\n", result.title.c_str());
    std::printf("description: %s\n", result.description.c_str());
    std::printf("developer: %s\n", result.developer.c_str());
    std::printf("publisher: %s\n", result.publisher.c_str());
    std::printf("release: %s\n", result.releaseDate.c_str());
    std::printf("genre: %s\n", result.genre.c_str());
    std::printf("players: %s\n", result.players.c_str());
    std::printf("rating: %s\n", result.rating.c_str());
    std::printf("cover: %s\n", result.coverUrl.c_str());
    std::printf("screenshot: %s\n", result.screenshotUrl.c_str());
    return 0;
}

void applySmbEnv(nascade::AppConfig& config) {
    overrideEnv("NASCADE_SMB_HOST", config.smb.host);
    overrideEnv("NASCADE_SMB_SHARE", config.smb.share);
    overrideEnv("NASCADE_SMB_ROOT", config.smb.root);
    overrideEnv("NASCADE_SMB_USER", config.smb.username);
    overrideEnv("NASCADE_SMB_PASSWORD", config.smb.password);
    overrideEnv("NASCADE_SMB_DOMAIN", config.smb.domain);
    const std::string port = nascade::util::getEnv("NASCADE_SMB_PORT");
    if (!port.empty()) config.smb.port = std::atoi(port.c_str());
}

int runSmbTest() {
#ifdef NASCADE_HAVE_LIBSMB2
    nascade::Paths paths = nascade::Paths::detect();
    paths.ensureDirs();
    nascade::Log::instance().init(nascade::util::joinPath(paths.logsDir, "Nascade.log"));

    nascade::AppConfig config = nascade::ConfigStore::load(paths);
    applySmbEnv(config);

    if (config.smb.host.empty() || config.smb.share.empty()) {
        std::fprintf(stderr, "缺少 NASCADE_SMB_HOST 或 NASCADE_SMB_SHARE\n");
        return 2;
    }

    nascade::SmbVfs vfs(config.smb);
    std::string error;
    if (!vfs.connect(error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    std::vector<nascade::VfsEntry> entries;
    if (!vfs.list("", entries, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        vfs.disconnect();
        return 1;
    }

    std::printf("SMB 连接成功：%s/%s，根目录 %zu 项，alive=%s\n",
                config.smb.host.c_str(), config.smb.share.c_str(), entries.size(),
                vfs.alive() ? "yes" : "no");
    const size_t limit = std::min<size_t>(entries.size(), 50);
    for (size_t i = 0; i < limit; ++i) {
        const auto& entry = entries[i];
        std::printf("%s  %s  %llu\n", entry.isDir ? "DIR " : "FILE",
                    entry.name.c_str(), static_cast<unsigned long long>(entry.size));
    }
    vfs.disconnect();
    return 0;
#else
    std::fprintf(stderr, "当前构建未启用 libsmb2 后端。\n");
    return 2;
#endif
}

int runDownloadTest() {
    nascade::Paths paths = nascade::Paths::detect();
    paths.ensureDirs();
    nascade::Log::instance().init(nascade::util::joinPath(paths.logsDir, "Nascade.log"));

    nascade::AppConfig config = nascade::ConfigStore::load(paths);
    applySmbEnv(config);

    const std::string localNasRoot = nascade::localNasRootFromEnv();
    const std::string remotePath = nascade::util::getEnv("NASCADE_REMOTE_PATH");
    const std::string localPath = nascade::util::getEnv("NASCADE_LOCAL_PATH");
    const std::string sizeText = nascade::util::getEnv("NASCADE_REMOTE_SIZE");

    if (remotePath.empty() || localPath.empty()) {
        std::fprintf(stderr, "缺少 NASCADE_REMOTE_PATH 或 NASCADE_LOCAL_PATH\n");
        return 2;
    }

    const uint64_t expectedSize = sizeText.empty()
        ? 0
        : static_cast<uint64_t>(std::strtoull(sizeText.c_str(), nullptr, 10));

    std::atomic<uint64_t> progress{0};
    const nascade::DownloadResult result = nascade::downloadFile(
        config.smb, localNasRoot, remotePath, localPath, expectedSize, &progress);

    if (!result.ok) {
        std::fprintf(stderr, "%s\n", result.error.c_str());
        return 1;
    }

    std::printf("下载完成：%s（%llu 字节%s）\n", localPath.c_str(),
                static_cast<unsigned long long>(result.bytes),
                result.resumed ? "，已续传" : "");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    SDL_SetMainReady();

    bool windowed = false;
    bool screenshot = false;
    bool testScraper = false;
    bool testScrape = false;
    bool testSmb = false;
    bool testDownload = false;
    bool testUiDownload = false;
    bool testUiScrape = false;
    std::string screenshotPath = "screenshots/home.ppm";
    std::string screenName = "home";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--windowed") {
            windowed = true;
        } else if (arg == "--screenshot") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--screenshot 需要一个输出文件路径\n");
                return 2;
            }
            screenshot = true;
            screenshotPath = argv[++i];
        } else if (arg == "--screen") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--screen 需要一个页面名称\n");
                return 2;
            }
            screenName = argv[++i];
        } else if (arg == "--test-scraper") {
            testScraper = true;
        } else if (arg == "--test-scrape") {
            testScrape = true;
        } else if (arg == "--test-smb") {
            testSmb = true;
        } else if (arg == "--test-download") {
            testDownload = true;
        } else if (arg == "--test-ui-download") {
            testUiDownload = true;
        } else if (arg == "--test-ui-scrape") {
            testUiScrape = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        } else {
            std::fprintf(stderr, "未知参数: %s\n", arg.c_str());
            printUsage();
            return 2;
        }
    }

    if (testScraper) return runScraperTest();
    if (testScrape) return runScrapeTest();
    if (testSmb) return runSmbTest();
    if (testDownload) return runDownloadTest();

    if (testUiScrape) {
        std::string platform = nascade::util::getEnv("NASCADE_SCRAPE_PLATFORM");
        if (platform.empty()) platform = "GBA";
        nascade::App app;
        std::string error;
        if (!app.init(true, true, error)) {
            std::fprintf(stderr, "初始化失败: %s\n", error.c_str());
            return 1;
        }
        const bool ok = app.runScrapeSelfTest(platform, error);
        app.shutdown();
        if (!ok) {
            std::fprintf(stderr, "UI 刮削自测失败: %s\n", error.c_str());
            return 1;
        }
        std::printf("UI 刮削自测成功\n");
        return 0;
    }

    if (testUiDownload) {
        const std::string remoteDir = nascade::util::getEnv("NASCADE_REMOTE_DIR");
        const std::string remoteFile = nascade::util::getEnv("NASCADE_REMOTE_FILE");
        if (remoteFile.empty()) {
            std::fprintf(stderr, "缺少 NASCADE_REMOTE_FILE\n");
            return 2;
        }
        nascade::App app;
        std::string error;
        if (!app.init(true, true, error)) {
            std::fprintf(stderr, "初始化失败: %s\n", error.c_str());
            return 1;
        }
        const bool ok = app.runDownloadSelfTest(remoteDir, remoteFile, error);
        app.shutdown();
        if (!ok) {
            std::fprintf(stderr, "UI 下载自测失败: %s\n", error.c_str());
            return 1;
        }
        std::printf("UI 下载自测成功\n");
        return 0;
    }

    nascade::App app;
    std::string error;
    if (!app.init(windowed, screenshot, error)) {
        std::fprintf(stderr, "Nascade 初始化失败: %s\n", error.c_str());
        return 1;
    }

    if (screenshot) {
        if (!app.renderScreenshot(screenshotPath, screenName, error)) {
            std::fprintf(stderr, "截图失败: %s\n", error.c_str());
            app.shutdown();
            return 1;
        }
        std::printf("截图已保存: %s\n", screenshotPath.c_str());
        app.shutdown();
        return 0;
    }

    const int result = app.run();
    app.shutdown();
    return result;
}
