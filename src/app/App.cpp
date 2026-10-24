#include "app/App.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <SDL_ttf.h>

#include "core/I18n.h"
#include "core/Log.h"
#include "core/Util.h"
#include "scan/GameScanner.h"
#include "scan/PlatformScanner.h"
#include "scrape/LibretroScraper.h"
#include "scrape/HttpClient.h"
#include "transfer/Downloader.h"
#include "vfs/VfsFactory.h"

#ifdef NASCADE_HAVE_LIBSMB2
#include "vfs/SmbVfs.h"
#endif

namespace nascade {

namespace {

constexpr int kWidth = 1024;
constexpr int kHeight = 768;
constexpr int kMargin = 48;
constexpr int kTopBar = 88;
constexpr int kBottomBar = 60;
constexpr int kContentHeight = 576;

template <typename T>
void abandonIfPending(std::future<T>& future) {
    if (!future.valid()) return;
    if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) return;
    // std::async 返回的 future 在析构时会等待任务结束。退出应用时不能让
    // 卡住的网络任务拖住主线程，因此把 shared state 移到堆上并故意不释放。
    auto* abandoned = new std::future<T>(std::move(future));
    (void)abandoned;
    Log::instance().warn("abandoned pending async task on shutdown");
}

const Color kCoverPalette[] = {
    {84, 120, 168, 255},
    {168, 104, 84, 255},
    {96, 148, 112, 255},
    {148, 112, 168, 255},
    {168, 148, 84, 255},
    {84, 148, 156, 255},
};

Color coverColor(int seed) {
    const int count = static_cast<int>(sizeof(kCoverPalette) / sizeof(kCoverPalette[0]));
    return kCoverPalette[((seed % count) + count) % count];
}

std::string firstUtf8Char(const std::string& value) {
    if (value.empty()) return "?";
    const unsigned char c = static_cast<unsigned char>(value[0]);
    size_t length = 1;
    if ((c & 0x80) == 0) length = 1;
    else if ((c & 0xE0) == 0xC0) length = 2;
    else if ((c & 0xF0) == 0xE0) length = 3;
    else if ((c & 0xF8) == 0xF0) length = 4;
    if (value.size() < length) length = 1;
    return value.substr(0, length);
}

std::vector<std::string> wrapText(Painter& painter, const std::string& text, int ptSize,
                                  int maxWidth, int maxLines) {
    std::vector<std::string> lines;
    if (text.empty() || maxLines <= 0) return lines;

    std::string line;
    for (size_t i = 0; i < text.size() && static_cast<int>(lines.size()) < maxLines;) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        size_t length = 1;
        if ((c & 0x80) == 0) length = 1;
        else if ((c & 0xE0) == 0xC0) length = 2;
        else if ((c & 0xF0) == 0xE0) length = 3;
        else if ((c & 0xF8) == 0xF0) length = 4;
        if (i + length > text.size()) length = 1;

        const std::string cp = text.substr(i, length);
        if (cp == "\n") {
            lines.push_back(line);
            line.clear();
            i += length;
            continue;
        }
        if (!line.empty() && painter.textWidth(line + cp, ptSize) > maxWidth) {
            lines.push_back(line);
            line = cp;
            if (static_cast<int>(lines.size()) >= maxLines) {
                line.clear();
                break;
            }
        } else {
            line += cp;
        }
        i += length;
    }
    if (!line.empty() && static_cast<int>(lines.size()) < maxLines) {
        lines.push_back(line);
    }
    return lines;
}

std::string joinStrings(const std::vector<std::string>& values, const std::string& separator) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out += separator;
        out += values[i];
    }
    return out;
}

std::string fnv1aHex(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(hash));
    return buffer;
}

}  // namespace

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init(bool windowed, bool hidden, std::string& error) {
    screenshotMode_ = hidden;
    paths_ = Paths::detect();
    paths_.ensureDirs();
    Log::instance().init(util::joinPath(paths_.logsDir, "Nascade.log"));
    Log::instance().info("Nascade starting; sdRoot=" + paths_.sdRoot);

    config_ = ConfigStore::load(paths_);
    if (!util::fileExists(paths_.configFile)) {
        ConfigStore::save(paths_, config_);
        Log::instance().info("default config written: " + paths_.configFile);
    }
    I18n::instance().setLanguage(config_.language);

    if (!initSdl(windowed, hidden, error)) {
        shutdown();
        return false;
    }
    if (!initFont(error)) {
        shutdown();
        return false;
    }

    painter_ = std::make_unique<Painter>(renderer_, fonts_);
    if (!hidden) input_.init();

    std::string dbError;
    if (!db_.open(util::joinPath(paths_.cacheDir, "cache.db"), dbError)) {
        Log::instance().warn("database open failed: " + dbError);
    }

    std::vector<std::string> caCandidates = {
        util::joinPath(paths_.appAssetsDir, "cacert.pem"),
        util::joinPath(paths_.sdRoot, "assets/cacert.pem"),
        util::joinPath(util::currentWorkingDir(), "assets/cacert.pem"),
    };
    for (const auto& candidate : caCandidates) {
        if (util::fileExists(candidate)) {
            HttpClient::setCaBundle(candidate);
            Log::instance().info("CA bundle: " + candidate);
            break;
        }
    }

    loadData();
    if (!screenshotMode_) {
        // 配置了 SMB 时在后台预连接，首页可以直接显示“已连接”和本地库统计。
        startRemoteConnect(false);
    }
    return true;
}

bool App::initSdl(bool windowed, bool hidden, std::string& error) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        error = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    Uint32 windowFlags = hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
    if (!windowed && !hidden && paths_.isDevice()) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow("Nascade", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWidth, kHeight, windowFlags);
    if (!window_) {
        error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        return false;
    }

    const Uint32 rendererFlags = hidden
        ? SDL_RENDERER_SOFTWARE
        : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    renderer_ = SDL_CreateRenderer(window_, -1, rendererFlags);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        return false;
    }
    SDL_RenderSetLogicalSize(renderer_, kWidth, kHeight);

    if (paths_.isDevice()) {
        SDL_ShowCursor(SDL_DISABLE);
    }
    return true;
}

bool App::initFont(std::string& error) {
    std::string lastError;
    for (const auto& candidate : paths_.fontCandidates()) {
        if (!util::fileExists(candidate)) continue;
        if (fonts_.init(candidate, lastError)) {
            Log::instance().info("font: " + candidate);
            return true;
        }
        Log::instance().warn("font load failed: " + candidate + " (" + lastError + ")");
    }

    error = "未找到可用字体。请将 NotoSansSC-Regular.otf 放到 assets/fonts/ 后重试。";
    if (!lastError.empty()) error += " 最后错误：" + lastError;
    return false;
}

void App::loadData() {
    platforms_ = PlatformScanner::scan(paths_);
    if (platformIndex_ < 0 || platformIndex_ >= static_cast<int>(platforms_.size())) {
        platformIndex_ = 0;
    }
}

void App::openPlatforms() {
    screen_ = Screen::Platforms;
    focus_ = platformIndex_;
    scroll_ = 0;
    ensureVisible(static_cast<int>(platforms_.size()), visibleRows(kContentHeight, 80), 80);
}

void App::openGames(int platformIndex) {
    if (platformIndex < 0 || platformIndex >= static_cast<int>(platforms_.size())) return;
    platformIndex_ = platformIndex;
    games_ = GameScanner::scan(platforms_[platformIndex]);
    platforms_[platformIndex].gameCount = static_cast<int>(games_.size());
    screen_ = Screen::Games;
    focus_ = 0;
    scroll_ = 0;
    gameIndex_ = 0;
}

void App::rescan() {
    loadData();
    clampFocus(static_cast<int>(platforms_.size()));
    scroll_ = 0;
    setToast(tr("toast.rescanned"));
}

void App::go(Screen screen, Screen returnTo) {
    screen_ = screen;
    returnScreen_ = returnTo;
    focus_ = 0;
    scroll_ = 0;
}

void App::saveConfig() {
    if (!ConfigStore::save(paths_, config_)) {
        Log::instance().error("failed to save config");
        setToast(tr("toast.saveFailed"));
    }
}

void App::setToast(const std::string& message) {
    toast_ = message;
    toastUntil_ = SDL_GetTicks() + 2800;
}

ScraperConfig App::effectiveScraperConfig() const {
    ScraperConfig scraperConfig = config_.scraper;
    const auto overrideEnv = [](const char* name, std::string& field) {
        const std::string value = util::getEnv(name);
        if (!value.empty()) field = value;
    };
    overrideEnv("NASCADE_SS_DEVID", scraperConfig.devId);
    overrideEnv("NASCADE_SS_DEVPASSWORD", scraperConfig.devPassword);
    overrideEnv("NASCADE_SS_SSID", scraperConfig.ssId);
    overrideEnv("NASCADE_SS_SSPASSWORD", scraperConfig.ssPassword);
    overrideEnv("NASCADE_SS_BASEURL", scraperConfig.baseUrl);
    overrideEnv("NASCADE_SCRAPER_PROVIDER", scraperConfig.provider);
    overrideEnv("NASCADE_LIBRETRO_BASEURL", scraperConfig.baseUrl);
    scraperConfig.provider = util::toLower(scraperConfig.provider);
    return scraperConfig;
}

void App::startScraperTest() {
    pollScraper();
    if (scraperRunning_) {
        setToast(tr("toast.scraperRunning"));
        return;
    }

    const ScraperConfig scraperConfig = effectiveScraperConfig();
    const bool libretro = scraperConfig.provider == "libretro";

    if (!libretro && (scraperConfig.devId.empty() || scraperConfig.devPassword.empty())) {
        setToast(tr("toast.noDevCreds"));
        scraperStatus_ = tr("toast.noDevCreds");
        return;
    }

    const std::string cacheDir = paths_.cacheDir;
    scraperStatus_ = tr("scraper.testing");
    scraperRunning_ = true;
    try {
        scraperFuture_ = std::async(std::launch::async, [scraperConfig, cacheDir, libretro]() {
            if (libretro) {
                LibretroScraper scraper(scraperConfig, cacheDir);
                return scraper.test();
            }
            ScreenScraper scraper(scraperConfig);
            return scraper.test();
        });
    } catch (const std::exception& e) {
        scraperRunning_ = false;
        scraperStatus_ = std::string("无法启动测试：") + e.what();
        setToast(scraperStatus_);
    }
}

void App::pollScraper() {
    if (!scraperFuture_.valid()) return;
    if (scraperFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    ScraperTestResult result = scraperFuture_.get();
    scraperRunning_ = false;
    scraperStatus_ = result.message;
    setToast(result.message);
    Log::instance().info(std::string("Scraper test ") + (result.ok ? "ok" : "failed") +
                         ": " + result.message);
}

void App::startScrape() {
    pollScrape();
    if (scrapeRunning_) {
        setToast(tr("scraper.scraping"));
        return;
    }
    if (games_.empty() || gameIndex_ < 0 || gameIndex_ >= static_cast<int>(games_.size())) {
        return;
    }

    const ScraperConfig scraperConfig = effectiveScraperConfig();
    const bool libretro = scraperConfig.provider == "libretro";
    if (!libretro && (scraperConfig.devId.empty() || scraperConfig.devPassword.empty())) {
        setToast(tr("toast.noDevCreds"));
        go(Screen::Scraper, Screen::Detail);
        return;
    }

    scrapeGameIndex_ = gameIndex_;
    const Game game = games_[gameIndex_];
    scrapeGamePath_ = game.path;
    const std::string mediaCacheDir = paths_.mediaCacheDir;
    const std::string cacheDir = paths_.cacheDir;
    scrapeStatus_ = tr("scraper.scraping");
    scrapeRunning_ = true;
    try {
        scrapeFuture_ = std::async(
            std::launch::async,
            [scraperConfig, game, mediaCacheDir, cacheDir, libretro]() {
            ScrapeOutcome outcome;
            if (libretro) {
                LibretroScraper scraper(scraperConfig, cacheDir);
                outcome.ok = scraper.fetch(game, outcome.data, outcome.error);
            } else {
                ScreenScraper scraper(scraperConfig);
                outcome.ok = scraper.fetch(game, outcome.data, outcome.error);
            }
            if (outcome.ok && !outcome.data.coverUrl.empty()) {
                const std::string cachePath = util::joinPath(
                    mediaCacheDir, "cover_" + fnv1aHex(outcome.data.coverUrl) + ".img");
                std::string imageError;
                if (HttpClient::download(outcome.data.coverUrl, cachePath, imageError, 90)) {
                    outcome.coverPath = cachePath;
                } else {
                    Log::instance().warn("cover download failed: " + imageError);
                    if (libretro) {
                        outcome.ok = false;
                        outcome.error = "封面下载失败：" + imageError;
                    }
                }
            }
            if (libretro && outcome.ok && outcome.coverPath.empty()) {
                outcome.ok = false;
                outcome.error = "未获得封面 URL";
            }
            return outcome;
        });
    } catch (const std::exception& e) {
        scrapeRunning_ = false;
        scrapeStatus_ = std::string("无法启动刮削：") + e.what();
        setToast(scrapeStatus_);
    }
}

void App::pollScrape() {
    if (!scrapeFuture_.valid()) return;
    if (scrapeFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    ScrapeOutcome outcome = scrapeFuture_.get();
    scrapeRunning_ = false;
    if (outcome.ok) {
        if (scrapeGameIndex_ >= 0 && scrapeGameIndex_ < static_cast<int>(games_.size()) &&
            games_[scrapeGameIndex_].path == scrapeGamePath_) {
            Game& game = games_[scrapeGameIndex_];
            if (!outcome.data.title.empty()) game.name = outcome.data.title;
            if (!outcome.data.description.empty()) game.description = outcome.data.description;
            if (!outcome.coverPath.empty()) {
                game.coverPath = outcome.coverPath;
                if (downloadRunning_ && scrapeGamePath_ == downloadRemotePath_) {
                    downloadCoverPath_ = outcome.coverPath;
                }
                for (const auto& platform : platforms_) {
                    if (platform.id != game.platformId) continue;
                    if (!platform.imgPath.empty() && !game.romName.empty()) {
                        const std::string target =
                            util::joinPath(platform.imgPath, game.romName + ".png");
                        std::string imageError;
                        if (saveScaledPng(outcome.coverPath, target, 300, imageError)) {
                            Log::instance().info("stock cover written: " + target);
                        } else {
                            Log::instance().warn("stock cover failed: " + imageError);
                        }
                    }
                    break;
                }
            }
        }
        scrapeStatus_ = tr("scraper.success") + "：" + outcome.data.title;
    } else {
        scrapeStatus_ = tr("scraper.failed") + "：" + outcome.error;
    }
    setToast(scrapeStatus_);
    Log::instance().info("Scrape result: " + scrapeStatus_);
}

SmbProfile App::effectiveSmbProfile() const {
    SmbProfile profile = config_.smb;
    const auto overrideEnv = [](const char* name, std::string& field) {
        const std::string value = util::getEnv(name);
        if (!value.empty()) field = value;
    };
    overrideEnv("NASCADE_SMB_HOST", profile.host);
    overrideEnv("NASCADE_SMB_SHARE", profile.share);
    overrideEnv("NASCADE_SMB_ROOT", profile.root);
    overrideEnv("NASCADE_SMB_USER", profile.username);
    overrideEnv("NASCADE_SMB_PASSWORD", profile.password);
    overrideEnv("NASCADE_SMB_DOMAIN", profile.domain);
    const std::string port = util::getEnv("NASCADE_SMB_PORT");
    if (!port.empty()) profile.port = std::atoi(port.c_str());
    return profile;
}

void App::startSmbTest() {
#ifdef NASCADE_HAVE_LIBSMB2
    pollSmb();
    if (smbRunning_) {
        setToast(tr("smb.testing"));
        return;
    }
    const SmbProfile profile = effectiveSmbProfile();
    if (profile.host.empty() || profile.share.empty()) {
        smbStatus_ = tr("smb.notConfigured");
        setToast(smbStatus_);
        return;
    }

    smbStatus_ = tr("smb.testing");
    smbRunning_ = true;
    try {
        smbFuture_ = std::async(std::launch::async, [profile]() {
            SmbVfs vfs(profile);
            std::string error;
            if (!vfs.connect(error)) return error;
            std::vector<VfsEntry> entries;
            if (!vfs.list("", entries, error)) {
                vfs.disconnect();
                return error;
            }
            vfs.disconnect();
            return std::string("SMB 连接成功，根目录 ") + std::to_string(entries.size()) + " 项。";
        });
    } catch (const std::exception& e) {
        smbRunning_ = false;
        smbStatus_ = std::string("无法启动 SMB 测试：") + e.what();
        setToast(smbStatus_);
    }
#else
    smbStatus_ = "当前构建未启用 libsmb2 后端。";
    setToast(smbStatus_);
#endif
}

void App::pollSmb() {
    if (!smbFuture_.valid()) return;
    if (smbFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    smbStatus_ = smbFuture_.get();
    smbRunning_ = false;
    setToast(smbStatus_);
    Log::instance().info("SMB test: " + smbStatus_);
}

void App::openRemote() {
    startRemoteConnect(true);
}

void App::startRemoteConnect(bool showScreen) {
    if (remoteVfs_ && remoteVfs_->connected()) {
        if (showScreen) {
            screen_ = Screen::Remote;
            loadRemoteDir(remoteCwd_);
        }
        return;
    }
    if (remoteConnecting_) {
        if (showScreen) {
            screen_ = Screen::Remote;
            remoteConnectSilent_ = false;
            focus_ = 0;
            scroll_ = 0;
        }
        return;
    }

    const SmbProfile profile = effectiveSmbProfile();
    const std::string localRoot = localNasRootFromEnv();
    if (localRoot.empty() && (profile.host.empty() || profile.share.empty())) {
        return;
    }

    std::string label;
    if (!localRoot.empty()) {
        label = tr("remote.localMock") + ": " + localRoot;
    } else {
        label = "smb://" + profile.host + "/" + profile.share;
        if (!profile.root.empty() && profile.root != "/") {
            std::string root = profile.root;
            if (root.front() != '/') root.insert(root.begin(), '/');
            label += root;
        }
    }

    remoteLabel_ = label;
    remoteError_.clear();
    remoteEntries_.clear();
    remoteCwd_.clear();
    remoteConnecting_ = true;
    remoteConnectSilent_ = !showScreen;
    if (showScreen) {
        screen_ = Screen::Remote;
        focus_ = 0;
        scroll_ = 0;
    }

    remoteConnectFuture_ = std::async(std::launch::async, [profile, localRoot, label]() {
        RemoteConnectOutcome outcome;
        outcome.vfs = makeVfs(profile, localRoot, outcome.error);
        outcome.ok = outcome.vfs != nullptr;
        outcome.label = label;
        return outcome;
    });
}

void App::pollRemoteConnect() {
    if (!remoteConnectFuture_.valid()) return;
    if (remoteConnectFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    RemoteConnectOutcome outcome = remoteConnectFuture_.get();
    const bool silent = remoteConnectSilent_;
    remoteConnecting_ = false;
    remoteConnectSilent_ = false;
    if (outcome.ok) {
        remoteVfs_ = std::move(outcome.vfs);
        remoteLabel_ = outcome.label;
        if (screen_ == Screen::Remote) {
            loadRemoteDir("");
        }
    } else {
        remoteError_ = outcome.error;
        if (!silent) setToast(outcome.error);
        Log::instance().warn("remote connect failed: " + outcome.error);
    }
}

void App::pollRemoteHealth() {
    const Uint32 now = SDL_GetTicks();
    if (now < remoteHealthCheckAt_) return;
    remoteHealthCheckAt_ = now + 1000;
    if (remoteConnecting_ || !remoteVfs_ || !remoteVfs_->connected()) return;
    if (remoteVfs_->alive()) return;

    Log::instance().warn("remote connection lost");
    remoteVfs_.reset();
    remoteLabel_.clear();
    remoteCwd_.clear();
    remoteEntries_.clear();
    remoteError_ = tr("remote.disconnected");
    setToast(remoteError_);
}

void App::loadRemoteDir(const std::string& path) {
    if (!remoteVfs_ || !remoteVfs_->connected()) {
        remoteError_ = "NAS 未连接";
        return;
    }
    for (const auto& part : util::split(path, '/')) {
        if (part == "..") {
            remoteError_ = "非法路径";
            return;
        }
    }

    std::vector<VfsEntry> entries;
    std::string error;
    if (!remoteVfs_->list(path, entries, error)) {
        remoteError_ = error;
        setToast(error);
        return;
    }

    remoteEntries_.clear();
    for (const auto& entry : entries) {
        if (entry.name.empty() || entry.name.front() == '.') continue;
        const std::string lower = util::toLower(entry.name);
        if (lower == "@eadir" || lower == "#recycle" ||
            lower == "system volume information" || lower == "thumbs.db") {
            continue;
        }
        if (util::endsWith(lower, ".db") || util::endsWith(lower, ".part")) continue;
        remoteEntries_.push_back(entry);
    }

    remoteCwd_ = path;
    remoteError_.clear();
    focus_ = 0;
    scroll_ = 0;
}

std::string App::joinRemote(const std::string& dir, const std::string& name) const {
    if (dir.empty()) return name;
    if (name.empty()) return dir;
    return dir + "/" + name;
}

std::string App::remoteParent(const std::string& path) const {
    if (path.empty()) return "";
    const size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return "";
    return path.substr(0, pos);
}

std::string App::remotePlatformFor(const std::string& fileName) const {
    const auto normalize = [](std::string value) {
        value = util::toLower(value);
        std::string out;
        for (char c : value) {
            if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(c);
        }
        return out;
    };

    const std::string dirKey = normalize(util::baseName(remoteCwd_));
    if (!dirKey.empty()) {
        for (const auto& platform : platforms_) {
            if (normalize(platform.id) == dirKey || normalize(platform.label) == dirKey) {
                return platform.id;
            }
        }
        static const std::map<std::string, std::string> aliases = {
            {"nintendoentertainmentsystem", "FC"},
            {"famicom", "FC"},
            {"supernintendo", "SFC"},
            {"superfamicom", "SFC"},
            {"gameboy", "GB"},
            {"gameboycolor", "GBC"},
            {"gameboyadvance", "GBA"},
            {"megadrive", "MD"},
            {"genesis", "MD"},
            {"playstation", "PS"},
            {"nintendo64", "N64"},
            {"pcengine", "PCE"},
            {"turbografx", "PCE"},
            {"playstationportable", "PSP"},
            {"dreamcast", "DC"},
        };
        const auto alias = aliases.find(dirKey);
        if (alias != aliases.end()) {
            for (const auto& platform : platforms_) {
                if (util::toLower(platform.id) == util::toLower(alias->second)) return platform.id;
            }
        }
    }

    const std::string ext = util::extensionLower(fileName);
    for (const auto& platform : platforms_) {
        if (std::find(platform.extensions.begin(), platform.extensions.end(), ext) !=
            platform.extensions.end()) {
            return platform.id;
        }
    }
    return "";
}

void App::openRemoteFile(int index) {
    if (index < 0 || index >= static_cast<int>(remoteEntries_.size())) return;
    const VfsEntry& entry = remoteEntries_[index];
    if (entry.isDir) {
        loadRemoteDir(joinRemote(remoteCwd_, entry.name));
        return;
    }

    Game game;
    game.name = util::baseNameNoExt(entry.name);
    game.romName = game.name;
    game.platformId = remotePlatformFor(entry.name);
    game.path = joinRemote(remoteCwd_, entry.name);
    game.format = util::extensionLower(entry.name);
    game.size = entry.size;
    game.mtime = entry.mtime;
    game.installed = false;
    game.remote = true;

    // NAS 游戏也使用本机 Imgs/<平台>/ 的封面；匹配规则与本地游戏一致。
    const Platform* coverPlatform = nullptr;
    for (const auto& candidate : platforms_) {
        if (candidate.id == game.platformId) {
            coverPlatform = &candidate;
            break;
        }
    }
    if (coverPlatform && !coverPlatform->imgPath.empty()) {
        CoverResolver covers(coverPlatform->imgPath);
        const std::string base = util::baseNameNoExt(entry.name);
        game.coverPath = covers.findDirect(base);
        if (game.coverPath.empty()) game.coverPath = covers.find(base);
        if (!game.coverPath.empty()) {
            Log::instance().info("remote cover: " + game.coverPath);
        }
    }
    if (coverPlatform && !coverPlatform->romPath.empty()) {
        const std::string localPath = util::joinPath(coverPlatform->romPath, entry.name);
        if (util::fileExists(localPath)) {
            game.installed = true;
            game.localPath = localPath;
        }
    }

    games_.clear();
    games_.push_back(std::move(game));
    gameIndex_ = 0;
    remoteSelectedIndex_ = index;
    remoteMode_ = true;
    scrapeStatus_.clear();
    downloadStatus_.clear();
    screen_ = Screen::Detail;
    focus_ = 0;
    scroll_ = 0;
}

void App::startDownload() {
    if (!remoteMode_) {
        setToast(tr("toast.localDownload"));
        return;
    }
    pollDownload();
    if (downloadRunning_) {
        setToast(tr("download.running"));
        return;
    }
    if (games_.empty() || gameIndex_ < 0 || gameIndex_ >= static_cast<int>(games_.size())) return;

    const Game& game = games_[gameIndex_];
    const std::string fileName = util::baseName(game.path);

    const Platform* platform = nullptr;
    for (const auto& candidate : platforms_) {
        if (candidate.id == game.platformId) {
            platform = &candidate;
            break;
        }
    }
    if (!platform) {
        setToast(tr("download.noPlatform"));
        return;
    }
    if (platform->romPath.empty()) {
        setToast(tr("download.noPlatform"));
        return;
    }

    const std::string destination = util::joinPath(platform->romPath, fileName);
    if (util::fileExists(destination)) {
        if (gameIndex_ >= 0 && gameIndex_ < static_cast<int>(games_.size())) {
            games_[gameIndex_].installed = true;
            games_[gameIndex_].localPath = destination;
        }
        setToast(tr("download.exists"));
        return;
    }
    if (!util::ensureDir(platform->romPath)) {
        downloadStatus_ = tr("download.failed") + "：无法创建目标目录";
        setToast(downloadStatus_);
        return;
    }

    const SmbProfile profile = effectiveSmbProfile();
    const std::string localRoot = localNasRootFromEnv();
    const std::string remotePath = game.path;
    const uint64_t expectedSize = game.size;
    downloadGameName_ = game.name;
    downloadCoverPath_ = game.coverPath;
    downloadRemotePath_ = game.path;
    downloadTargetPath_ = destination;
    downloadTotal_ = expectedSize;
    downloadDone_->store(0);
    downloadRunning_ = true;
    downloadStatus_ = tr("download.running");
    const auto progress = downloadDone_;

    try {
        downloadFuture_ = std::async(
            std::launch::async,
            [profile, localRoot, remotePath, destination, expectedSize, progress]() {
                DownloadOutcome outcome;
                const DownloadResult result = downloadFile(
                    profile, localRoot, remotePath, destination, expectedSize, progress.get());
                outcome.ok = result.ok;
                outcome.error = result.error;
                return outcome;
            });
    } catch (const std::exception& e) {
        downloadRunning_ = false;
        downloadStatus_ = tr("download.failed") + "：" + e.what();
        setToast(downloadStatus_);
    }
}

void App::pollDownload() {
    if (!downloadFuture_.valid()) return;
    if (downloadFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    DownloadOutcome outcome = downloadFuture_.get();
    downloadRunning_ = false;
    if (outcome.ok) {
        downloadStatus_ = tr("download.installed") + "：" + downloadTargetPath_ +
                          " · " + tr("download.refreshHint");
        if (remoteMode_ && gameIndex_ >= 0 && gameIndex_ < static_cast<int>(games_.size()) &&
            games_[gameIndex_].path == downloadRemotePath_) {
            games_[gameIndex_].installed = true;
            games_[gameIndex_].localPath = downloadTargetPath_;
        }
    } else {
        downloadStatus_ = tr("download.failed") + "：" + outcome.error;
    }
    setToast(downloadStatus_);
    Log::instance().info("download result: " + downloadStatus_);
}

void App::confirmDeleteGame() {
    deleteConfirm_ = false;
    const int index = deleteTargetIndex_;
    deleteTargetIndex_ = -1;
    if (index < 0 || index >= static_cast<int>(games_.size())) return;

    const Game game = games_[index];
    const std::string mainPath = game.localPath.empty() ? game.path : game.localPath;
    if (mainPath.empty() || !util::fileExists(mainPath)) {
        setToast(tr("toast.deleteFailed"));
        return;
    }

    std::vector<std::string> paths{mainPath};
    const std::string ext = util::extensionLower(mainPath);
    if (ext == "cue" || ext == "gdi" || ext == "m3u") {
        // 常见光盘镜像的附属文件；同目录同名才删除，避免误删其他游戏。
        static const char* kCompanions[] = {
            "bin", "img", "iso", "ccd", "sub", "raw", "track", "wav", "mp3", "ogg", "flac",
        };
        const std::string parent = util::parentPath(mainPath);
        const std::string base = util::baseNameNoExt(mainPath);
        for (const char* companion : kCompanions) {
            const std::string path = util::joinPath(parent, base + "." + companion);
            if (path != mainPath && util::fileExists(path)) paths.push_back(path);
        }
    }

    bool mainDeleted = false;
    for (const auto& path : paths) {
        if (std::remove(path.c_str()) == 0) {
            if (path == mainPath) mainDeleted = true;
        } else {
            Log::instance().warn("delete failed: " + path);
        }
    }
    if (!mainDeleted) {
        setToast(tr("toast.deleteFailed"));
        return;
    }

    games_.erase(games_.begin() + index);
    if (platformIndex_ >= 0 && platformIndex_ < static_cast<int>(platforms_.size()) &&
        platforms_[platformIndex_].gameCount > 0) {
        --platforms_[platformIndex_].gameCount;
    }

    screen_ = Screen::Games;
    if (games_.empty()) {
        focus_ = 0;
        gameIndex_ = 0;
        scroll_ = 0;
    } else {
        focus_ = std::min(index, static_cast<int>(games_.size()) - 1);
        gameIndex_ = focus_;
        ensureVisible(static_cast<int>(games_.size()), visibleRows(kContentHeight, 80), 80);
    }

    setToast(tr("toast.deleted") + "：" + game.name + " · " + tr("download.refreshHint"));
    Log::instance().info("deleted: " + mainPath);
}

int App::run() {
    running_ = true;
    while (running_) {
        const Uint32 frameStart = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            const Action action = input_.handle(event);
            if (action != Action::None) handle(action);
            if (!running_) break;
        }

        // 长按上下键时由 Input 产生重复事件，仅在列表/网格页面应用。
        if (screen_ == Screen::Home || screen_ == Screen::Platforms ||
            screen_ == Screen::Games || screen_ == Screen::Remote) {
            const Action repeated = input_.repeat();
            if (repeated != Action::None) handle(repeated);
        }

        pollScraper();
        pollScrape();
        pollSmb();
        pollRemoteConnect();
        pollRemoteHealth();
        pollDownload();
        draw();

        const Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }
    return 0;
}

void App::shutdown() {
    if (shutdownDone_) return;
    shutdownDone_ = true;
    Log::instance().info("Nascade shutdown");
    input_.shutdown();
    scraperRunning_ = false;
    scrapeRunning_ = false;
    smbRunning_ = false;
    downloadRunning_ = false;
    remoteConnecting_ = false;
    abandonIfPending(scraperFuture_);
    abandonIfPending(scrapeFuture_);
    abandonIfPending(smbFuture_);
    abandonIfPending(remoteConnectFuture_);
    abandonIfPending(downloadFuture_);
    painter_.reset();
    images_.clear();
    fonts_.clear();
    db_.close();
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    SDL_Quit();
}

void App::handle(Action action) {
    if (action == Action::Quit) {
        running_ = false;
        return;
    }

    switch (screen_) {
        case Screen::Home: handleHome(action); break;
        case Screen::Platforms: handlePlatforms(action); break;
        case Screen::Games: handleGames(action); break;
        case Screen::Detail: handleDetail(action); break;
        case Screen::Settings: handleSettings(action); break;
        case Screen::About: handleAbout(action); break;
        case Screen::Smb: handleSmb(action); break;
        case Screen::Scraper: handleScraper(action); break;
        case Screen::Logs: handleLogs(action); break;
        case Screen::Remote: handleRemote(action); break;
    }
}

void App::handleHome(Action action) {
    if (action == Action::Up) {
        if (focus_ == HomeConnect) {
            // already at the top
        } else if (focus_ <= HomeBrowse) {
            focus_ = HomeConnect;
        } else {
            focus_ -= 2;
        }
    } else if (action == Action::Down) {
        if (focus_ == HomeConnect) {
            focus_ = HomeNas;
        } else if (focus_ + 2 < HomeItemCount) {
            focus_ += 2;
        }
    } else if (action == Action::Left) {
        // 网格从 HomeNas 开始、两列排列；右列（偏移为奇数）才能左移。
        if (focus_ > HomeConnect && (focus_ - HomeNas) % 2 == 1) --focus_;
    } else if (action == Action::Right) {
        // 左列（偏移为偶数）右移进入同一行的右列，不跨行回绕。
        if (focus_ > HomeConnect && (focus_ - HomeNas) % 2 == 0 && focus_ + 1 < HomeItemCount) {
            ++focus_;
        }
    } else if (action == Action::PageUp) {
        focus_ = HomeConnect;
    } else if (action == Action::PageDown) {
        focus_ = HomeQuit;
    } else if (action == Action::Option) {
        go(Screen::Smb, Screen::Home);
    } else if (action == Action::Menu) {
        go(Screen::Settings, Screen::Home);
    } else if (action == Action::Back) {
        running_ = false;
    } else if (action == Action::Confirm) {
        switch (focus_) {
            case HomeConnect: {
                // 已连接/已配置时卡片展示统计或下载进度，A 不再跳转 SMB；
                // 未连接时才把卡片当作连接入口。
                const bool active = remoteVfs_ && remoteVfs_->connected();
                const bool configured = !config_.smb.host.empty() && !config_.smb.share.empty();
                if (!active && !configured) {
                    go(Screen::Smb, Screen::Home);
                }
                break;
            }
            case HomeNas: openRemote(); break;
            case HomeBrowse: openPlatforms(); break;
            case HomeScrape:
                if (scraperStatus_.empty()) scraperStatus_ = tr("scraper.notConfigured");
                go(Screen::Scraper, Screen::Home);
                break;
            case HomeSettings: go(Screen::Settings, Screen::Home); break;
            case HomeAbout: go(Screen::About, Screen::Home); break;
            case HomeQuit: running_ = false; break;
            default: break;
        }
    }
}

void App::handlePlatforms(Action action) {
    const int count = static_cast<int>(platforms_.size());
    if (action == Action::Up) --focus_;
    else if (action == Action::Down) ++focus_;
    else if (action == Action::PageUp) focus_ = 0;
    else if (action == Action::PageDown) focus_ = count > 0 ? count - 1 : 0;
    else if (action == Action::Confirm) {
        if (!platforms_.empty()) openGames(focus_);
        return;
    } else if (action == Action::Back) {
        go(Screen::Home, Screen::Home);
        return;
    } else if (action == Action::Option) {
        rescan();
        return;
    }

    clampFocus(count);
    ensureVisible(count, visibleRows(kContentHeight, 80), 80);
}

void App::handleGames(Action action) {
    const int count = static_cast<int>(games_.size());
    if (action == Action::Up) --focus_;
    else if (action == Action::Down) ++focus_;
    else if (action == Action::PageUp) focus_ = 0;
    else if (action == Action::PageDown) focus_ = count > 0 ? count - 1 : 0;
    else if (action == Action::Confirm) {
        if (!games_.empty()) {
            gameIndex_ = focus_;
            remoteMode_ = false;
            scrapeStatus_.clear();
            downloadStatus_.clear();
            screen_ = Screen::Detail;
            focus_ = 0;
            scroll_ = 0;
        }
        return;
    } else if (action == Action::Back) {
        openPlatforms();
        return;
    } else if (action == Action::Option) {
        setToast(tr("toast.localDownload"));
        return;
    }

    clampFocus(count);
    ensureVisible(count, visibleRows(kContentHeight, 80), 80);
}

void App::handleDetail(Action action) {
    if (deleteConfirm_) {
        if (action == Action::Back) {
            deleteConfirm_ = false;
            deleteTargetIndex_ = -1;
        } else if (action == Action::Left || action == Action::Up) {
            deleteFocus_ = 0;
        } else if (action == Action::Right || action == Action::Down) {
            deleteFocus_ = 1;
        } else if (action == Action::Confirm) {
            if (deleteFocus_ == 0) {
                confirmDeleteGame();
            } else {
                deleteConfirm_ = false;
                deleteTargetIndex_ = -1;
            }
        }
        return;
    }

    const int buttonCount = remoteMode_ ? 2 : 3;
    if (focus_ < 0 || focus_ >= buttonCount) focus_ = 0;
    if (action == Action::Left || action == Action::Up) {
        focus_ = (focus_ + buttonCount - 1) % buttonCount;
    } else if (action == Action::Right || action == Action::Down) {
        focus_ = (focus_ + 1) % buttonCount;
    } else if (action == Action::Confirm) {
        if (focus_ == 0) {
            if (remoteMode_) {
                const bool installed = gameIndex_ >= 0 &&
                    gameIndex_ < static_cast<int>(games_.size()) &&
                    games_[gameIndex_].installed;
                if (installed) {
                    const std::string& path = games_[gameIndex_].localPath;
                    setToast(tr("download.installed") + "：" + path);
                } else {
                    startDownload();
                }
            } else {
                setToast(tr("toast.localGame"));
            }
        } else if (focus_ == 1) {
            startScrape();
        } else if (focus_ == 2 && !remoteMode_) {
            if (gameIndex_ >= 0 && gameIndex_ < static_cast<int>(games_.size())) {
                deleteConfirm_ = true;
                deleteTargetIndex_ = gameIndex_;
                deleteFocus_ = 1;  // 默认选中“取消”，避免误删
            }
        }
    } else if (action == Action::Back) {
        if (remoteMode_) {
            screen_ = Screen::Remote;
            remoteMode_ = false;
            focus_ = remoteSelectedIndex_;
            clampFocus(static_cast<int>(remoteEntries_.size()));
            ensureVisible(static_cast<int>(remoteEntries_.size()),
                          visibleRows(kContentHeight, 80), 80);
        } else {
            screen_ = Screen::Games;
            focus_ = gameIndex_;
            scroll_ = std::max(0, gameIndex_ - visibleRows(kContentHeight, 80) + 1);
        }
    }
}

void App::handleSettings(Action action) {
    constexpr int kSettingCount = 8;
    if (action == Action::Up) --focus_;
    else if (action == Action::Down) ++focus_;
    else if (action == Action::PageUp) focus_ = 0;
    else if (action == Action::PageDown) focus_ = kSettingCount - 1;
    else if (action == Action::Left || action == Action::Right) {
        const int delta = action == Action::Right ? 1 : -1;
        if (focus_ == 0) {
            config_.language = (config_.language == "zh") ? "en" : "zh";
            I18n::instance().setLanguage(config_.language);
            saveConfig();
        } else if (focus_ == 1) {
            const double values[] = {0.9, 1.0, 1.15};
            int index = 1;
            double best = 999.0;
            for (int i = 0; i < 3; ++i) {
                const double diff = std::fabs(config_.fontScale - values[i]);
                if (diff < best) { best = diff; index = i; }
            }
            index = std::max(0, std::min(2, index + delta));
            config_.fontScale = values[index];
            saveConfig();
        } else if (focus_ == 2) {
            config_.gridView = !config_.gridView;
            saveConfig();
            if (config_.gridView) setToast(tr("toast.gridLater"));
        }
    } else if (action == Action::Confirm) {
        switch (focus_) {
            case 0:
                config_.language = (config_.language == "zh") ? "en" : "zh";
                I18n::instance().setLanguage(config_.language);
                saveConfig();
                break;
            case 1:
                config_.fontScale = config_.fontScale < 1.05 ? 1.15 : 1.0;
                saveConfig();
                break;
            case 2:
                config_.gridView = !config_.gridView;
                saveConfig();
                if (config_.gridView) setToast(tr("toast.gridLater"));
                break;
            case 3: go(Screen::Smb, Screen::Settings); break;
            case 4: go(Screen::Scraper, Screen::Settings); break;
            case 5: go(Screen::Logs, Screen::Settings); break;
            case 6: go(Screen::About, Screen::Settings); break;
            case 7: go(Screen::Home, Screen::Home); break;
            default: break;
        }
    } else if (action == Action::Back) {
        go(Screen::Home, Screen::Home);
    }

    clampFocus(kSettingCount);
}

void App::handleAbout(Action action) {
    if (action == Action::Back || action == Action::Confirm) {
        screen_ = returnScreen_;
        focus_ = 0;
        scroll_ = 0;
    }
}

void App::handleSmb(Action action) {
    if (action == Action::Confirm) {
        startSmbTest();
    } else if (action == Action::Back) {
        screen_ = returnScreen_;
        focus_ = 0;
        scroll_ = 0;
    }
}

void App::handleScraper(Action action) {
    constexpr int kScraperItems = 3;
    const auto togglePreference = [this]() {
        if (config_.scraper.provider == "libretro") {
            const std::string& region = config_.scraper.region;
            if (region == "eu") {
                config_.scraper.region = "jp";
            } else if (region == "jp") {
                config_.scraper.region = "us";
            } else {
                config_.scraper.region = "eu";
            }
        } else {
            config_.scraper.language = (config_.scraper.language == "zh") ? "en" : "zh";
        }
        saveConfig();
    };

    if (action == Action::Up) --focus_;
    else if (action == Action::Down) ++focus_;
    else if (action == Action::Left || action == Action::Right) {
        if (focus_ == 1) togglePreference();
    } else if (action == Action::Confirm) {
        if (focus_ == 0) {
            startScraperTest();
        } else if (focus_ == 1) {
            togglePreference();
        } else if (focus_ == 2) {
            screen_ = returnScreen_;
            focus_ = 0;
            scroll_ = 0;
        }
    } else if (action == Action::Back) {
        screen_ = returnScreen_;
        focus_ = 0;
        scroll_ = 0;
    }
    clampFocus(kScraperItems);
}

void App::handleLogs(Action action) {
    if (action == Action::Confirm) {
        const std::string source = util::joinPath(paths_.logsDir, "Nascade.log");
        const std::string target = util::joinPath(paths_.sdRoot, "Nascade.log");
        std::string data;
        if (util::readFile(source, data) && util::writeFile(target, data)) {
            setToast(tr("toast.exported"));
        } else {
            setToast(tr("toast.exportFailed"));
        }
    } else if (action == Action::Back) {
        screen_ = returnScreen_;
        focus_ = 0;
        scroll_ = 0;
    }
}

void App::handleRemote(Action action) {
    if (remoteConnecting_) {
        if (action == Action::Back) {
            screen_ = Screen::Home;
            focus_ = HomeNas;
        }
        return;
    }

    if (!remoteError_.empty()) {
        if (action == Action::Confirm || action == Action::Option) {
            // 重试时丢弃旧会话，重新建立连接，避免卡在失效的 VFS 上。
            remoteVfs_.reset();
            remoteLabel_.clear();
            remoteCwd_.clear();
            remoteEntries_.clear();
            remoteError_.clear();
            const SmbProfile profile = effectiveSmbProfile();
            if (localNasRootFromEnv().empty() &&
                (profile.host.empty() || profile.share.empty())) {
                remoteError_ = tr("smb.notConfigured");
                return;
            }
            openRemote();
        } else if (action == Action::Back) {
            screen_ = Screen::Home;
            focus_ = HomeNas;
        }
        return;
    }

    const int count = static_cast<int>(remoteEntries_.size());
    if (action == Action::Up) --focus_;
    else if (action == Action::Down) ++focus_;
    else if (action == Action::PageUp) focus_ = 0;
    else if (action == Action::PageDown) focus_ = count > 0 ? count - 1 : 0;
    else if (action == Action::Confirm) {
        if (focus_ >= 0 && focus_ < count) {
            const VfsEntry& entry = remoteEntries_[focus_];
            if (entry.isDir) {
                loadRemoteDir(joinRemote(remoteCwd_, entry.name));
            } else {
                openRemoteFile(focus_);
            }
        }
        return;
    } else if (action == Action::Back) {
        if (remoteCwd_.empty()) {
            screen_ = Screen::Home;
            focus_ = HomeNas;
        } else {
            loadRemoteDir(remoteParent(remoteCwd_));
        }
        return;
    } else if (action == Action::Option) {
        loadRemoteDir(remoteCwd_);
        return;
    }

    clampFocus(count);
    ensureVisible(count, visibleRows(kContentHeight, 80), 80);
}

void App::draw() {
    drawScene();
    painter_->end();
}

void App::drawScene() {
    painter_->begin(theme::kBg);

    switch (screen_) {
        case Screen::Home: drawHome(); break;
        case Screen::Platforms: drawPlatforms(); break;
        case Screen::Games: drawGames(); break;
        case Screen::Detail: drawDetail(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::About: drawAbout(); break;
        case Screen::Smb: drawSmb(); break;
        case Screen::Scraper: drawScraper(); break;
        case Screen::Logs: drawLogs(); break;
        case Screen::Remote: drawRemote(); break;
    }

    if (deleteConfirm_ && screen_ == Screen::Detail) {
        drawDeleteConfirm();
    }
    drawToast();
}

void App::drawTopBar(const std::string& title, const std::string& subtitle) {
    painter_->fillRect(0, 0, kWidth, kTopBar, theme::kBg);
    painter_->fillRect(0, kTopBar - 1, kWidth, 1, theme::kBorder);
    painter_->fillRect(kMargin, 22, 6, 44, theme::kAccent);

    painter_->textIn(title, 72, 14, fontSize(30), theme::kText,
                     SDL_Rect{72, 8, 700, 40});
    // 主标题与副标题之间留出约一行字形的呼吸空间，避免贴在一起。
    painter_->textIn(subtitle, 72, 61, fontSize(15), theme::kMuted,
                     SDL_Rect{72, 55, 800, 30});

    const bool smbActive = remoteVfs_ && remoteVfs_->connected();
    const bool smbConfigured = !config_.smb.host.empty() && !config_.smb.share.empty();
    std::string status;
    Color statusColor = theme::kMuted;
    if (smbActive) {
        status = tr("home.connected");
        statusColor = theme::kGreen;
    } else if (remoteConnecting_) {
        status = tr("home.connecting");
        statusColor = theme::kAccent;
    } else if (smbConfigured) {
        status = tr("home.notConnected");
    } else {
        status = tr("home.notConfigured");
    }
    painter_->textIn("SMB  " + status, kWidth - 48, 30, fontSize(14), statusColor,
                     SDL_Rect{kWidth - 260, 22, 212, 26}, Align::Right);
}

void App::drawBottomBar(const std::vector<std::string>& hints) {
    const int y = kHeight - kBottomBar;
    painter_->fillRect(0, y, kWidth, kBottomBar, theme::kBg);
    painter_->fillRect(0, y, kWidth, 1, theme::kBorder);
    painter_->text(joinStrings(hints, "     "), kMargin, y + 20, fontSize(15), theme::kMuted);
}

void App::drawHome() {
    drawTopBar(tr("app.name"), tr("app.tagline"));

    const bool heroFocused = focus_ == HomeConnect;
    const Color heroBg = heroFocused ? theme::kPanelHover : theme::kPanel;
    painter_->roundedRect(48, 108, 928, 150, 18, heroBg);
    if (heroFocused) {
        painter_->roundedOutline(48, 108, 928, 150, 18, 2, theme::kAccent, heroBg);
    }
    painter_->fillRect(48, 108, 6, 150, theme::kAccent);

    const auto statusBadge = [&](const std::string& text, Color bg, Color fg) {
        const int textSize = fontSize(15);
        const int textHeight = painter_->textHeight(text, textSize);
        painter_->roundedRect(788, 132, 156, 34, 10, bg);
        painter_->text(text, 866, 132 + (34 - (textHeight > 0 ? textHeight : textSize)) / 2,
                       textSize, fg, Align::Center);
    };

    int totalGames = 0;
    for (const auto& platform : platforms_) totalGames += platform.gameCount;

    const bool activeConnection = remoteVfs_ && remoteVfs_->connected();
    const bool smbConfigured = !config_.smb.host.empty() && !config_.smb.share.empty();
    const bool connected = activeConnection || smbConfigured;
    if (downloadRunning_) {
        const uint64_t done = downloadDone_->load();
        const uint64_t total = downloadTotal_;
        const int percent = total > 0
            ? std::min(100, static_cast<int>(done * 100 / total))
            : 0;
        const bool complete = total > 0 && done >= total;

        statusBadge(tr("home.downloading"), theme::kAccentSoft, theme::kAccent);

        const int coverX = 64;
        const int coverY = 131;
        const int coverW = 104;
        const int coverH = 104;
        bool coverDrawn = false;
        if (!downloadCoverPath_.empty()) {
            int imageWidth = 0;
            int imageHeight = 0;
            SDL_Texture* texture =
                images_.load(renderer_, downloadCoverPath_, imageWidth, imageHeight);
            if (texture && imageWidth > 0 && imageHeight > 0) {
                const double scale = std::min(static_cast<double>(coverW) / imageWidth,
                                              static_cast<double>(coverH) / imageHeight);
                const int drawW = std::max(1, static_cast<int>(std::lround(imageWidth * scale)));
                const int drawH = std::max(1, static_cast<int>(std::lround(imageHeight * scale)));
                painter_->roundedRect(coverX, coverY, coverW, coverH, 12, theme::kPanelAlt);
                SDL_Rect destination{coverX + (coverW - drawW) / 2,
                                     coverY + (coverH - drawH) / 2, drawW, drawH};
                SDL_RenderCopy(painter_->raw(), texture, nullptr, &destination);
                coverDrawn = true;
            }
        }
        if (!coverDrawn) {
            painter_->roundedRect(coverX, coverY, coverW, coverH, 12, coverColor(0));
            const std::string initial = firstUtf8Char(
                downloadGameName_.empty() ? tr("home.nas") : downloadGameName_);
            painter_->text(initial, coverX + coverW / 2, coverY + 30, fontSize(40),
                           Color{235, 240, 245, 255}, Align::Center);
        }

        painter_->textIn(tr("home.downloadingTitle"), 188, 128, fontSize(30), theme::kText,
                         SDL_Rect{188, 122, 560, 42});
        painter_->textIn(downloadGameName_.empty() ? tr("home.nas") : downloadGameName_,
                         188, 174, fontSize(18), theme::kMuted,
                         SDL_Rect{188, 170, 560, 26});

        if (complete) {
            painter_->textIn(tr("download.finishing"), 188, 214, fontSize(15),
                             theme::kAccent, SDL_Rect{188, 210, 540, 24});
        } else {
            painter_->roundedRect(188, 206, 540, 10, 5, theme::kPanelAlt);
            if (total > 0) {
                painter_->roundedRect(188, 206, 540 * percent / 100, 10, 5, theme::kAccent);
            }
            std::string info = tr("download.running") + "  " + util::formatSize(done);
            if (total > 0) {
                info += " / " + util::formatSize(total) + "  " + std::to_string(percent) + "%";
            }
            painter_->textIn(info, 188, 222, fontSize(14), theme::kAccent,
                             SDL_Rect{188, 218, 540, 22});
        }

        if (total > 0) {
            painter_->text(std::to_string(percent) + "%", 962, 178, fontSize(32),
                           theme::kAccent, Align::Right);
            painter_->text(util::formatSize(total), 962, 220, fontSize(14),
                           theme::kMuted, Align::Right);
        }
    } else if (connected) {
        painter_->textIn(tr("home.libraryTitle"), 88, 128, fontSize(30), theme::kText,
                         SDL_Rect{88, 122, 560, 42});
        const std::string stats = std::to_string(platforms_.size()) + " " +
                                  tr("home.platforms") + " · " +
                                  std::to_string(totalGames) + " " + tr("home.games");
        painter_->textIn(stats, 88, 196, fontSize(26), theme::kText,
                         SDL_Rect{88, 190, 660, 38});
    } else {
        painter_->textIn(tr("home.heroTitle"), 88, 128, fontSize(30), theme::kText,
                         SDL_Rect{88, 122, 560, 38});
        painter_->textIn(tr("home.heroDesc"), 88, 176, fontSize(16), theme::kMuted,
                         SDL_Rect{88, 170, 620, 48});

        const bool configured = !config_.smb.host.empty();
        statusBadge(configured ? tr("home.configured") : tr("home.notConfigured"),
                    configured ? theme::kAccentSoft : theme::kPanelAlt,
                    configured ? theme::kGreen : theme::kMuted);

        const std::string counts = std::to_string(platforms_.size()) + " " + tr("home.platforms") +
                                   " · " + std::to_string(totalGames) + " " + tr("home.games");
        painter_->text(counts, 962, 188, fontSize(15), theme::kMuted, Align::Right);
        painter_->text(heroFocused ? tr("home.heroHint") : tr("home.heroDesc"),
                       88, 218, fontSize(14), heroFocused ? theme::kAccent : theme::kMuted);
    }

    static const char* titles[] = {
        "home.nas", "home.local", "home.scrape",
        "home.settings", "home.about", "home.quit",
    };
    static const char* descriptions[] = {
        "home.nasDesc", "home.localDesc", "home.scrapeDesc",
        "home.settingsDesc", "home.aboutDesc", "home.quitDesc",
    };

    for (int i = 0; i < 6; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        const int x = 48 + col * 476;
        const int y = 286 + row * 132;
        // 瓷砖顺序与 HomeItem 枚举一致：NAS、本机、刮削、设置、关于、退出。
        const int item = HomeNas + i;
        const bool focused = focus_ == item;
        const Color bg = focused ? theme::kPanelHover : theme::kPanel;

        painter_->roundedRect(x, y, 452, 116, 16, bg);
        if (focused) painter_->roundedOutline(x, y, 452, 116, 16, 2, theme::kAccent, bg);
        painter_->fillRect(x + 18, y + 24, 4, 68, focused ? theme::kAccent : theme::kBorder);
        painter_->text(tr(titles[i]), x + 40, y + 24, fontSize(24), theme::kText);
        const char* descriptionKey = descriptions[i];
        if (i == 2 && config_.scraper.provider == "libretro") {
            descriptionKey = "home.scrapeDescLibretro";
        }
        painter_->textIn(tr(descriptionKey), x + 40, y + 62, fontSize(15), theme::kMuted,
                         SDL_Rect{x + 40, y + 58, 360, 40});
        painter_->text(tr("common.arrow"), x + 452 - 26, y + 44, fontSize(22),
                       focused ? theme::kAccent : theme::kMuted, Align::Right);
    }

    drawBottomBar({tr("nav.confirm"), tr("nav.back"), tr("nav.connectLibrary"), tr("nav.menu")});
}

void App::drawPlatforms() {
    drawTopBar(tr("platforms.title"), tr("platforms.subtitle"));

    if (platforms_.empty()) {
        painter_->roundedRect(48, 108, 928, 240, 18, theme::kPanel);
        painter_->text(tr("platforms.empty"), 512, 176, fontSize(28), theme::kText, Align::Center);
        painter_->textIn(tr("platforms.emptyHint"), 512, 222, fontSize(17), theme::kMuted,
                         SDL_Rect{96, 216, 832, 28}, Align::Center);
        drawBottomBar({tr("nav.back"), tr("platforms.refresh")});
        return;
    }

    const int visible = visibleRows(kContentHeight, 80);
    ensureVisible(static_cast<int>(platforms_.size()), visible, 80);

    for (int i = scroll_; i < static_cast<int>(platforms_.size()) && i < scroll_ + visible; ++i) {
        const int y = 116 + (i - scroll_) * 80;
        const bool focused = i == focus_;
        const Color bg = focused ? theme::kPanelHover : theme::kPanel;
        painter_->roundedRect(48, y, 320, 72, 14, bg);
        if (focused) painter_->roundedOutline(48, y, 320, 72, 14, 2, theme::kAccent, bg);
        painter_->fillRect(62, y + 16, 4, 40, focused ? theme::kAccent : theme::kBorder);
        painter_->textIn(platforms_[i].label, 78, y + 12, fontSize(22), theme::kText,
                         SDL_Rect{78, y + 8, 268, 30});
        painter_->text(std::to_string(platforms_[i].gameCount) + " " + tr("platforms.count"),
                       78, y + 42, fontSize(14), theme::kMuted);
    }

    painter_->roundedRect(388, 108, 588, 576, 18, theme::kPanel);

    const int index = std::max(0, std::min(focus_, static_cast<int>(platforms_.size()) - 1));
    const Platform& platform = platforms_[index];
    painter_->textIn(platform.label, 416, 132, fontSize(38), theme::kText,
                     SDL_Rect{416, 126, 430, 48});
    painter_->text(std::to_string(platform.gameCount) + " " + tr("platforms.count"),
                   944, 146, fontSize(16), theme::kMuted, Align::Right);

    const auto field = [&](const std::string& label, const std::string& value, int y) {
        painter_->text(label, 416, y, fontSize(14), theme::kMuted);
        painter_->textIn(value, 416, y + 26, fontSize(17), theme::kText,
                         SDL_Rect{416, y + 22, 532, 26});
    };

    field(tr("platforms.romPath"), platform.romPath, 210);
    field(tr("platforms.imgPath"), platform.imgPath, 286);
    field(tr("platforms.exts"), joinStrings(platform.extensions, ", "), 362);

    painter_->text(tr("platforms.open"), 416, 620, fontSize(18), theme::kAccent);
    drawBottomBar({tr("nav.confirm"), tr("nav.back"), tr("nav.option")});
}

void App::drawGames() {
    const std::string platformLabel = (platformIndex_ >= 0 && platformIndex_ < static_cast<int>(platforms_.size()))
        ? platforms_[platformIndex_].label
        : tr("games.title");
    drawTopBar(platformLabel, std::to_string(games_.size()) + " " + tr("platforms.count"));

    if (games_.empty()) {
        painter_->roundedRect(48, 108, 928, 220, 18, theme::kPanel);
        painter_->text(tr("games.empty"), 512, 176, fontSize(26), theme::kText, Align::Center);
        painter_->text(tr("games.detail"), 512, 220, fontSize(16), theme::kMuted, Align::Center);
        drawBottomBar({tr("nav.back")});
        return;
    }

    const int visible = visibleRows(kContentHeight, 80);
    ensureVisible(static_cast<int>(games_.size()), visible, 80);

    for (int i = scroll_; i < static_cast<int>(games_.size()) && i < scroll_ + visible; ++i) {
        const Game& game = games_[i];
        const std::string subtitle = game.format + "  ·  " + util::formatSize(game.size) +
                                     "  ·  " + tr("games.local");
        drawRow(48, 116 + (i - scroll_) * 80, 928, 72, i == focus_, game.name, subtitle,
                game.installed ? tr("games.installed") : "", i);
    }

    drawBottomBar({tr("nav.confirm"), tr("nav.back"), tr("nav.option")});
}

void App::drawDetail() {
    if (games_.empty() || gameIndex_ < 0 || gameIndex_ >= static_cast<int>(games_.size())) {
        drawTopBar(tr("detail.title"), tr("common.none"));
        painter_->roundedRect(48, 108, 928, 220, 18, theme::kPanel);
        painter_->text(tr("games.empty"), 512, 190, fontSize(26), theme::kText, Align::Center);
        drawBottomBar({tr("nav.back")});
        return;
    }

    const Game& game = games_[gameIndex_];
    drawTopBar(tr("detail.title"), "");

    painter_->roundedRect(48, 112, 280, 280, 18, theme::kPanel);
    bool coverDrawn = false;
    if (!game.coverPath.empty()) {
        int imageWidth = 0;
        int imageHeight = 0;
        SDL_Texture* texture = images_.load(renderer_, game.coverPath, imageWidth, imageHeight);
        if (texture && imageWidth > 0 && imageHeight > 0) {
            const int boxX = 64;
            const int boxY = 128;
            const int boxW = 248;
            const int boxH = 248;
            const double scale = std::min(static_cast<double>(boxW) / imageWidth,
                                          static_cast<double>(boxH) / imageHeight);
            const int drawW = std::max(1, static_cast<int>(std::lround(imageWidth * scale)));
            const int drawH = std::max(1, static_cast<int>(std::lround(imageHeight * scale)));
            SDL_Rect destination{boxX + (boxW - drawW) / 2, boxY + (boxH - drawH) / 2,
                                 drawW, drawH};
            SDL_RenderCopy(painter_->raw(), texture, nullptr, &destination);
            coverDrawn = true;
        }
    }
    if (!coverDrawn) {
        painter_->roundedRect(64, 128, 248, 248, 14, coverColor(gameIndex_));
        painter_->text(firstUtf8Char(game.name), 188, 212, fontSize(72),
                       Color{235, 240, 245, 255}, Align::Center);
    }

    painter_->textIn(game.name, 360, 112, fontSize(34), theme::kText,
                     SDL_Rect{360, 106, 616, 46});

    int badgeX = 360;
    const int badgeY = 166;
    const auto badge = [&](const std::string& text, Color bg, Color fg) {
        const int textSize = fontSize(14);
        const int width = painter_->textWidth(text, textSize) + 24;
        const int textHeight = painter_->textHeight(text, textSize);
        painter_->roundedRect(badgeX, badgeY, width, 30, 8, bg);
        painter_->text(text, badgeX + width / 2,
                       badgeY + (30 - (textHeight > 0 ? textHeight : textSize)) / 2,
                       textSize, fg, Align::Center);
        badgeX += width + 10;
    };
    badge(game.platformId, theme::kAccentSoft, theme::kAccent);
    if (!game.format.empty()) badge(game.format, theme::kPanelAlt, theme::kMuted);
    if (remoteMode_ && game.installed) {
        badge(tr("games.installed"), theme::kPanelAlt, theme::kGreen);
    }

    painter_->text(tr("detail.size"), 360, 218, fontSize(14), theme::kMuted);
    painter_->text(util::formatSize(game.size), 440, 214, fontSize(18), theme::kText);
    painter_->text(tr("detail.platform"), 600, 218, fontSize(14), theme::kMuted);
    painter_->text(game.platformId, 680, 214, fontSize(18), theme::kText);

    painter_->text(tr(remoteMode_ ? "detail.remotePath" : "detail.path"),
                   360, 270, fontSize(14), theme::kMuted);
    painter_->textIn(game.path, 360, 296, fontSize(16), theme::kText,
                     SDL_Rect{360, 292, 616, 24});

    painter_->text(tr("detail.description"), 360, 348, fontSize(14), theme::kMuted);
    const std::string description = game.description.empty()
        ? tr(config_.scraper.provider == "libretro" ? "detail.noDescriptionLibretro"
                                                    : "detail.noDescription")
        : game.description;
    const auto lines = wrapText(*painter_, description, fontSize(17), 616, 4);
    int lineY = 376;
    for (const auto& line : lines) {
        painter_->textIn(line, 360, lineY, fontSize(17), theme::kMuted,
                         SDL_Rect{360, lineY, 616, fontSize(17) + 6});
        lineY += fontSize(17) + 6;
    }

    const bool currentDownload = remoteMode_ && !downloadRemotePath_.empty() &&
                                 game.path == downloadRemotePath_;
    const bool showDownloadProgress = currentDownload && downloadRunning_;
    const uint64_t downloadDone = showDownloadProgress ? downloadDone_->load() : 0;
    const bool downloadComplete =
        showDownloadProgress && downloadTotal_ > 0 && downloadDone >= downloadTotal_;
    if (showDownloadProgress && !downloadComplete) {
        const uint64_t total = downloadTotal_;
        const int percent = total > 0
            ? std::min(100, static_cast<int>(downloadDone * 100 / total))
            : 0;

        painter_->roundedRect(360, 500, 616, 10, 5, theme::kPanelAlt);
        if (total > 0) {
            painter_->roundedRect(360, 500, 616 * percent / 100, 10, 5, theme::kAccent);
        }

        std::string info = tr("download.running") + "  " + util::formatSize(downloadDone);
        if (total > 0) {
            info += " / " + util::formatSize(total) + "  " + std::to_string(percent) + "%";
        }
        painter_->textIn(info, 360, 520, fontSize(15), theme::kAccent,
                         SDL_Rect{360, 516, 616, 24});
    } else if (showDownloadProgress) {
        // 字节数已到 100%，但后台还在关闭连接/重命名；此时不再画 100% 进度条。
        painter_->textIn(tr("download.finishing"), 360, 520, fontSize(15), theme::kAccent,
                         SDL_Rect{360, 516, 616, 24});
    } else if (currentDownload && !downloadStatus_.empty()) {
        painter_->textIn(downloadStatus_, 360, 520, fontSize(15), theme::kMuted,
                         SDL_Rect{360, 516, 616, 24});
    }

    if (!scrapeStatus_.empty() && scrapeGamePath_ == game.path) {
        painter_->textIn(scrapeStatus_, 360, 562, fontSize(15),
                         scrapeRunning_ ? theme::kAccent : theme::kMuted,
                         SDL_Rect{360, 558, 616, 24});
    }

    // 本机游戏没有“下载安装”动作；远程游戏下载完成后显示“已安装”。
    if (remoteMode_) {
        const bool installed = game.installed;
        const bool currentDownloadRunning = currentDownload && downloadRunning_;
        const std::string primaryLabel = installed
            ? tr("games.installed")
            : (currentDownloadRunning ? tr("download.running") : tr("detail.download"));
        drawButton(360, 594, 280, 52, focus_ == 0, primaryLabel, !installed);
        drawButton(660, 594, 280, 52, focus_ == 1, tr("detail.scrape"), false);
    } else {
        drawButton(360, 594, 190, 52, focus_ == 0, tr("detail.localGame"), false);
        drawButton(570, 594, 190, 52, focus_ == 1, tr("detail.scrape"), false);
        drawButton(780, 594, 190, 52, focus_ == 2, tr("detail.delete"), false, true);
    }

    if (!deleteConfirm_) {
        drawBottomBar({tr("nav.confirm"), tr("nav.back")});
    }
}

void App::drawDeleteConfirm() {
    if (deleteTargetIndex_ < 0 ||
        deleteTargetIndex_ >= static_cast<int>(games_.size())) {
        deleteConfirm_ = false;
        deleteTargetIndex_ = -1;
        return;
    }
    const Game& game = games_[deleteTargetIndex_];

    painter_->fillRect(0, 0, kWidth, kHeight, theme::kOverlay);
    const int x = 212;
    const int y = 214;
    const int w = 600;
    const int h = 310;
    painter_->roundedRect(x, y, w, h, 18, theme::kPanel);
    painter_->roundedOutline(x, y, w, h, 18, 2, theme::kRed, theme::kPanel);

    painter_->textIn(tr("detail.deleteTitle"), x + w / 2, y + 28, fontSize(28),
                     theme::kText, SDL_Rect{x + 30, y + 24, w - 60, 42},
                     Align::Center);
    painter_->textIn(game.name, x + w / 2, y + 86, fontSize(22), theme::kRed,
                     SDL_Rect{x + 40, y + 82, w - 80, 34}, Align::Center);
    painter_->textIn(game.path, x + w / 2, y + 128, fontSize(14), theme::kMuted,
                     SDL_Rect{x + 40, y + 124, w - 80, 22}, Align::Center);

    const auto lines = wrapText(*painter_, tr("detail.deleteWarning"),
                                fontSize(16), w - 80, 2);
    int lineY = y + 168;
    for (const auto& line : lines) {
        painter_->textIn(line, x + w / 2, lineY, fontSize(16), theme::kMuted,
                         SDL_Rect{x + 40, lineY - 2, w - 80, fontSize(16) + 8},
                         Align::Center);
        lineY += fontSize(16) + 6;
    }

    drawButton(x + 60, y + 236, 200, 52, deleteFocus_ == 0,
               tr("detail.delete"), false, true);
    drawButton(x + 340, y + 236, 200, 52, deleteFocus_ == 1,
               tr("common.cancel"), false);
}

void App::drawSettings() {
    drawTopBar(tr("settings.title"), tr("home.settingsDesc"));

    static const char* titleKeys[] = {
        "settings.language", "settings.fontScale", "settings.view", "settings.smb",
        "settings.scrape", "settings.logs", "settings.about", "settings.back",
    };

    std::string fontValue = tr("settings.value.medium");
    if (config_.fontScale < 0.95) fontValue = tr("settings.value.small");
    else if (config_.fontScale > 1.05) fontValue = tr("settings.value.large");

    std::string values[8];
    values[0] = config_.language == "en" ? tr("settings.value.en") : tr("settings.value.zh");
    values[1] = fontValue;
    values[2] = config_.gridView ? tr("settings.value.grid") : tr("settings.value.list");
    values[3] = "";
    values[4] = "";
    values[5] = "";
    values[6] = "";
    values[7] = "";

    for (int i = 0; i < 8; ++i) {
        const int y = 116 + i * 66;
        const bool focused = focus_ == i;
        const Color bg = focused ? theme::kPanelHover : theme::kPanel;
        painter_->roundedRect(48, y, 928, 58, 12, bg);
        if (focused) painter_->roundedOutline(48, y, 928, 58, 12, 2, theme::kAccent, bg);

        const std::string title = tr(titleKeys[i]);
        const int titleSize = fontSize(20);
        const int titleHeight = painter_->textHeight(title, titleSize);
        painter_->text(title, 76, y + (58 - (titleHeight > 0 ? titleHeight : titleSize)) / 2,
                       titleSize, theme::kText);

        if (i <= 2) {
            const int valueSize = fontSize(18);
            const int valueHeight = painter_->textHeight(values[i], valueSize);
            painter_->text(values[i], 944,
                           y + (58 - (valueHeight > 0 ? valueHeight : valueSize)) / 2,
                           valueSize, theme::kAccent, Align::Right);
        } else {
            const int arrowSize = fontSize(20);
            const std::string arrow = tr("common.arrow");
            const int arrowHeight = painter_->textHeight(arrow, arrowSize);
            painter_->text(arrow, 944,
                           y + (58 - (arrowHeight > 0 ? arrowHeight : arrowSize)) / 2,
                           arrowSize, theme::kMuted, Align::Right);
        }
    }

    drawBottomBar({tr("nav.adjust"), tr("nav.enter"), tr("nav.back")});
}

void App::drawAbout() {
    drawTopBar(tr("about.title"), tr("home.aboutDesc"));
    painter_->roundedRect(48, 108, 928, 576, 18, theme::kPanel);

    painter_->text(tr("app.name"), 80, 136, fontSize(32), theme::kText);
    painter_->text(tr("app.version"), 80, 182, fontSize(18), theme::kAccent);

    const auto row = [&](const std::string& label, const std::string& value, int y) {
        painter_->text(label, 80, y, fontSize(16), theme::kMuted);
        painter_->textIn(value, 300, y - 2, fontSize(20), theme::kText,
                         SDL_Rect{300, y - 6, 640, 30});
    };

    row(tr("about.target"), tr("about.targetValue"), 250);
    row(tr("about.protocol"), tr("about.protocolValue"), 314);
    row(tr("about.license"), tr("about.licenseValue"), 378);
    row(tr("about.dataSource"),
        config_.scraper.provider == "libretro" ? "thumbnails.libretro.com" : "ScreenScraper.fr",
        442);

    painter_->textIn(tr("about.licenseText"), 80, 530, fontSize(16), theme::kMuted,
                     SDL_Rect{80, 526, 864, 28});
    painter_->textIn(tr("about.disclaimer"), 80, 570, fontSize(16), theme::kAccent,
                     SDL_Rect{80, 566, 864, 28});

    drawBottomBar({tr("nav.back")});
}

void App::drawSmb() {
    drawTopBar(tr("smb.title"), tr("about.protocolValue"));
    painter_->roundedRect(48, 108, 928, 576, 18, theme::kPanel);

    const auto row = [&](const std::string& label, const std::string& value, int y) {
        painter_->text(label, 80, y, fontSize(16), theme::kMuted);
        painter_->textIn(value, 340, y - 2, fontSize(19), theme::kText,
                         SDL_Rect{340, y - 6, 580, 30});
    };

    row(tr("smb.host"), config_.smb.host.empty() ? tr("smb.notConfigured") : config_.smb.host, 150);
    row(tr("smb.share"), config_.smb.share.empty() ? tr("common.none") : config_.smb.share, 214);
    row(tr("smb.root"), config_.smb.root.empty() ? "/" : config_.smb.root, 278);
    row(tr("smb.user"), config_.smb.username.empty() ? tr("common.none") : config_.smb.username, 342);
    std::string passwordValue = tr("smb.notSaved");
    if (!config_.smb.password.empty()) {
        passwordValue = config_.smb.savePassword ? tr("smb.saved") : tr("smb.notSaved");
    }
    row(tr("smb.password"), passwordValue, 406);

    if (!smbStatus_.empty()) {
        painter_->textIn(smbStatus_, 80, 468, fontSize(15),
                         smbRunning_ ? theme::kAccent : theme::kMuted,
                         SDL_Rect{80, 464, 864, 24});
    }

    drawButton(80, 500, 864, 56, focus_ == 0, tr("smb.test"), true);
    painter_->textIn(tr("smb.placeholder"), 80, 586, fontSize(15), theme::kMuted,
                     SDL_Rect{80, 582, 864, 40});

    drawBottomBar({tr("nav.confirm"), tr("nav.back")});
}

void App::drawScraper() {
    const bool libretro = config_.scraper.provider == "libretro";
    drawTopBar(tr("scraper.title"),
               libretro ? tr("scraper.libretroSubtitle") : tr("home.scrapeDesc"));
    painter_->roundedRect(48, 108, 928, 576, 18, theme::kPanel);

    const auto row = [&](const std::string& label, const std::string& value, int y) {
        painter_->text(label, 80, y, fontSize(16), theme::kMuted);
        painter_->textIn(value, 340, y - 2, fontSize(19), theme::kText,
                         SDL_Rect{340, y - 6, 580, 30});
    };

    row(tr("scraper.provider"), libretro ? tr("scraper.libretroProvider") : "ScreenScraper", 146);
    const std::string credentialValue = libretro
        ? tr("scraper.noCredentials")
        : (config_.scraper.devId.empty()
               ? tr("scraper.notConfigured")
               : (config_.scraper.ssId.empty() ? tr("scraper.devOnly")
                                               : tr("scraper.devAndUser")));
    row(tr("scraper.credentials"), credentialValue, 202);

    std::string status = scraperStatus_;
    if (status.empty()) {
        if (libretro) {
            status = config_.scraper.baseUrl.empty() ? tr("scraper.notConfigured")
                                                     : tr("home.configured");
        } else {
            status = config_.scraper.devId.empty() ? tr("scraper.notConfigured")
                                                   : tr("home.configured");
        }
    }
    row(tr("scraper.status"), status, 258);

    const auto regionLabel = [this](const std::string& region) -> std::string {
        if (region == "eu") return tr("scraper.region.eu");
        if (region == "jp") return tr("scraper.region.jp");
        return tr("scraper.region.us");
    };
    const std::string preferenceLabel = libretro
        ? regionLabel(config_.scraper.region)
        : (config_.scraper.language == "en" ? "English" : "简体中文");
    painter_->text(libretro ? tr("scraper.coverRegion") : tr("scraper.language"),
                   80, 320, fontSize(16), theme::kMuted);
    painter_->text(preferenceLabel, 340, 318, fontSize(19), theme::kText);

    const char* hintKey = libretro ? "scraper.libretroHint" : "scraper.notConfiguredHint";
    const auto hintLines = wrapText(*painter_, tr(hintKey), fontSize(15), 840, 3);
    int hintY = 370;
    for (const auto& line : hintLines) {
        painter_->text(line, 80, hintY, fontSize(15), theme::kMuted);
        hintY += fontSize(15) + 5;
    }

    drawButton(80, 448, 864, 54, focus_ == 0,
               libretro ? tr("scraper.testLibretro") : tr("scraper.test"), true);

    const Color languageBg = focus_ == 1 ? theme::kPanelHover : theme::kPanelAlt;
    painter_->roundedRect(80, 518, 864, 52, 12, languageBg);
    if (focus_ == 1) painter_->roundedOutline(80, 518, 864, 52, 12, 2, theme::kAccent, languageBg);
    const int prefSize = fontSize(18);
    const std::string prefLabel = libretro ? tr("scraper.coverRegion") : tr("scraper.language");
    const int prefLabelHeight = painter_->textHeight(prefLabel, prefSize);
    const int prefValueHeight = painter_->textHeight(preferenceLabel, prefSize);
    painter_->text(prefLabel, 104, 518 + (52 - (prefLabelHeight > 0 ? prefLabelHeight : prefSize)) / 2,
                   prefSize, theme::kText);
    painter_->text(preferenceLabel, 920,
                   518 + (52 - (prefValueHeight > 0 ? prefValueHeight : prefSize)) / 2,
                   prefSize, theme::kAccent, Align::Right);

    const Color backBg = focus_ == 2 ? theme::kPanelHover : theme::kPanelAlt;
    painter_->roundedRect(80, 584, 864, 52, 12, backBg);
    if (focus_ == 2) painter_->roundedOutline(80, 584, 864, 52, 12, 2, theme::kAccent, backBg);
    const std::string backLabel = tr("common.back");
    const int backHeight = painter_->textHeight(backLabel, prefSize);
    painter_->text(backLabel, 104, 584 + (52 - (backHeight > 0 ? backHeight : prefSize)) / 2,
                   prefSize, theme::kText);

    drawBottomBar({tr("nav.confirm"), tr("nav.adjust"), tr("nav.back")});
}

void App::drawLogs() {
    drawTopBar(tr("logs.title"), tr("logs.subtitle"));
    painter_->roundedRect(48, 108, 928, 576, 18, theme::kPanel);

    painter_->text(tr("logs.path"), 80, 150, fontSize(16), theme::kMuted);
    painter_->textIn(util::joinPath(paths_.logsDir, "Nascade.log"), 300, 148,
                     fontSize(18), theme::kText, SDL_Rect{300, 144, 640, 28});

    const auto lines = wrapText(*painter_, tr("logs.hint"), fontSize(16), 840, 3);
    int y = 210;
    for (const auto& line : lines) {
        painter_->text(line, 80, y, fontSize(16), theme::kMuted);
        y += fontSize(16) + 6;
    }

    drawButton(80, 340, 864, 54, focus_ == 0, tr("logs.export"), true);
    painter_->text(tr("logs.exportHint"), 80, 416, fontSize(15), theme::kMuted);

    drawBottomBar({tr("nav.confirm"), tr("nav.back")});
}

void App::drawRemote() {
    const std::string subtitle = remoteCwd_.empty() ? remoteLabel_
                                                     : (remoteLabel_ + " / " + remoteCwd_);
    drawTopBar(tr("remote.title"), subtitle);

    if (remoteConnecting_) {
        painter_->roundedRect(48, 108, 928, 240, 18, theme::kPanel);
        painter_->text(tr("remote.connecting"), 512, 200, fontSize(26), theme::kText, Align::Center);
        painter_->text(remoteLabel_, 512, 244, fontSize(16), theme::kMuted, Align::Center);
        drawBottomBar({tr("nav.back")});
        return;
    }

    if (!remoteError_.empty()) {
        painter_->roundedRect(48, 108, 928, 340, 18, theme::kPanel);
        const auto lines = wrapText(*painter_, remoteError_, fontSize(18), 820, 4);
        int y = 170;
        for (const auto& line : lines) {
            painter_->text(line, 512, y, fontSize(18), theme::kMuted, Align::Center);
            y += fontSize(18) + 8;
        }
        drawBottomBar({tr("remote.retry"), tr("nav.back")});
        return;
    }

    if (remoteEntries_.empty()) {
        painter_->roundedRect(48, 108, 928, 220, 18, theme::kPanel);
        painter_->text(tr("remote.empty"), 512, 180, fontSize(26), theme::kText, Align::Center);
        painter_->text(tr("remote.up"), 512, 224, fontSize(16), theme::kMuted, Align::Center);
        drawBottomBar({tr("nav.back"), tr("remote.reload")});
        return;
    }

    const int visible = visibleRows(kContentHeight, 80);
    ensureVisible(static_cast<int>(remoteEntries_.size()), visible, 80);

    for (int i = scroll_; i < static_cast<int>(remoteEntries_.size()) && i < scroll_ + visible; ++i) {
        const VfsEntry& entry = remoteEntries_[i];
        const bool isDir = entry.isDir;
        const std::string title = isDir ? entry.name : util::baseNameNoExt(entry.name);
        const std::string format = util::extensionLower(entry.name);
        const std::string platform = isDir ? "" : remotePlatformFor(entry.name);

        std::string rowSubtitle;
        std::string right;
        if (isDir) {
            rowSubtitle = tr("remote.dir");
            right = tr("common.arrow");
        } else {
            rowSubtitle = format + "  ·  " + util::formatSize(entry.size) + "  ·  " +
                          (platform.empty() ? tr("remote.unknownPlatform") : platform);
            right = platform.empty() ? tr("remote.file") : platform;
        }

        drawRow(48, 116 + (i - scroll_) * 80, 928, 72, i == focus_, title,
                rowSubtitle, right, i);
    }

    drawBottomBar({tr("nav.confirm"), tr("nav.back"), tr("nav.option")});
}

void App::drawToast() {
    if (toast_.empty() || SDL_GetTicks() >= toastUntil_) return;

    const int textWidth = painter_->textWidth(toast_, fontSize(16));
    const int width = std::min(880, textWidth + 56);
    const int height = 46;
    const int x = (kWidth - width) / 2;
    const int y = 632;

    painter_->roundedRect(x, y, width, height, 12, theme::kPanelAlt);
    painter_->roundedOutline(x, y, width, height, 12, 1, theme::kBorder, theme::kPanelAlt);
    painter_->textIn(toast_, x + width / 2, y + 12, fontSize(16), theme::kText,
                     SDL_Rect{x + 24, y + 10, width - 48, 26}, Align::Center);
}

void App::drawRow(int x, int y, int w, int h, bool focused, const std::string& title,
                  const std::string& subtitle, const std::string& right, int seed) {
    const Color bg = focused ? theme::kPanelHover : theme::kPanel;
    painter_->roundedRect(x, y, w, h, 14, bg);
    if (focused) painter_->roundedOutline(x, y, w, h, 14, 2, theme::kAccent, bg);

    const int coverSize = h - 20;
    const int coverX = x + 10;
    const int coverY = y + 10;
    painter_->roundedRect(coverX, coverY, coverSize, coverSize, 10, coverColor(seed));
    painter_->text(firstUtf8Char(title), coverX + coverSize / 2, coverY + 8,
                   fontSize(26), Color{18, 22, 28, 255}, Align::Center);

    const int textX = coverX + coverSize + 16;
    const int clipWidth = w - (textX - x) - (right.empty() ? 24 : 150);
    const SDL_Rect clip{textX, y + 8, clipWidth, h - 16};
    painter_->textIn(title, textX, y + 10, fontSize(23), theme::kText, clip);
    painter_->textIn(subtitle, textX, y + 42, fontSize(15), theme::kMuted, clip);

    if (!right.empty()) {
        const Color rightColor = right == tr("games.installed") ? theme::kGreen : theme::kMuted;
        painter_->text(right, x + w - 18, y + (h - fontSize(15)) / 2, fontSize(15),
                       rightColor, Align::Right);
    }
}

void App::drawButton(int x, int y, int w, int h, bool focused, const std::string& label,
                     bool primary, bool danger) {
    Color bg;
    Color fg;
    Color outline = theme::kAccent;
    if (danger) {
        bg = focused ? theme::kRed : theme::kPanelAlt;
        fg = focused ? Color{20, 24, 30, 255} : theme::kRed;
        outline = theme::kRed;
    } else if (primary) {
        bg = focused ? theme::kAccent : theme::kAccentSoft;
        fg = focused ? Color{20, 24, 30, 255} : theme::kAccent;
    } else {
        bg = focused ? theme::kPanelHover : theme::kPanel;
        fg = theme::kText;
    }

    painter_->roundedRect(x, y, w, h, 12, bg);
    if (focused) painter_->roundedOutline(x, y, w, h, 12, 2, outline, bg);
    const int textSize = fontSize(18);
    const int textHeight = painter_->textHeight(label, textSize);
    const int textY = y + (h - (textHeight > 0 ? textHeight : textSize)) / 2;
    painter_->text(label, x + w / 2, textY, textSize, fg, Align::Center);
}

int App::fontSize(int base) const {
    return std::max(12, static_cast<int>(std::lround(base * config_.fontScale)));
}

int App::visibleRows(int contentHeight, int rowHeight) const {
    return std::max(1, contentHeight / rowHeight);
}

void App::clampFocus(int count) {
    if (count <= 0) {
        focus_ = 0;
    } else {
        focus_ = std::max(0, std::min(focus_, count - 1));
    }
}

void App::ensureVisible(int count, int visible, int rowHeight) {
    (void)rowHeight;
    if (count <= 0) {
        scroll_ = 0;
        focus_ = 0;
        return;
    }
    clampFocus(count);
    if (focus_ < scroll_) scroll_ = focus_;
    if (focus_ >= scroll_ + visible) scroll_ = focus_ - visible + 1;
    scroll_ = std::max(0, std::min(scroll_, std::max(0, count - visible)));
}

bool App::runDownloadSelfTest(const std::string& remoteDir, const std::string& fileName,
                              std::string& error) {
    const std::string localRoot = localNasRootFromEnv();
    std::string connectError;
    remoteVfs_ = makeVfs(effectiveSmbProfile(), localRoot, connectError);
    if (!remoteVfs_) {
        error = connectError;
        return false;
    }
    remoteLabel_ = localRoot.empty() ? "SMB" : (tr("remote.localMock") + ": " + localRoot);

    loadRemoteDir(remoteDir);
    if (!remoteError_.empty()) {
        error = remoteError_;
        return false;
    }

    int index = -1;
    for (int i = 0; i < static_cast<int>(remoteEntries_.size()); ++i) {
        if (!remoteEntries_[i].isDir && remoteEntries_[i].name == fileName) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        error = "远程文件不存在: " + fileName;
        return false;
    }

    openRemoteFile(index);
    startDownload();
    if (!downloadRunning_) {
        error = downloadStatus_.empty() ? "下载未启动" : downloadStatus_;
        return false;
    }

    const Uint32 start = SDL_GetTicks();
    while (downloadRunning_ && SDL_GetTicks() - start < 30000) {
        pollDownload();
        SDL_Delay(20);
    }
    if (downloadRunning_) {
        error = "下载超时";
        return false;
    }
    if (!downloadStatus_.empty() &&
        downloadStatus_.find(tr("download.installed")) == std::string::npos) {
        error = downloadStatus_;
        return false;
    }
    return true;
}

bool App::runScrapeSelfTest(const std::string& platformId, std::string& error) {
    for (int i = 0; i < static_cast<int>(platforms_.size()); ++i) {
        if (platforms_[i].id != platformId) continue;
        platformIndex_ = i;
        games_ = GameScanner::scan(platforms_[i]);
        if (games_.empty()) {
            error = "平台没有游戏: " + platformId;
            return false;
        }
        gameIndex_ = 0;
        remoteMode_ = false;
        screen_ = Screen::Detail;
        focus_ = 0;
        scrapeStatus_.clear();
        startScrape();
        if (!scrapeRunning_) {
            error = scrapeStatus_.empty() ? "刮削未启动" : scrapeStatus_;
            return false;
        }
        const Uint32 start = SDL_GetTicks();
        while (scrapeRunning_ && SDL_GetTicks() - start < 60000) {
            pollScrape();
            SDL_Delay(20);
        }
        if (scrapeRunning_) {
            error = "刮削超时";
            return false;
        }
        if (scrapeStatus_.find(tr("scraper.success")) == std::string::npos) {
            error = scrapeStatus_;
            return false;
        }
        return true;
    }
    error = "未找到平台: " + platformId;
    return false;
}

bool App::renderScreenshot(const std::string& path, const std::string& screenName,
                           std::string& error) {
    if (!renderer_ || !painter_) {
        error = "renderer is not initialized";
        return false;
    }

    if (screenName == "platforms") {
        screen_ = Screen::Platforms;
        focus_ = 0;
        scroll_ = 0;
    } else if (screenName == "games" || screenName == "detail") {
        if (!platforms_.empty()) {
            platformIndex_ = 0;
            const std::string wanted = util::getEnv("NASCADE_SCREENSHOT_PLATFORM");
            if (!wanted.empty()) {
                for (int i = 0; i < static_cast<int>(platforms_.size()); ++i) {
                    if (platforms_[i].id == wanted) {
                        platformIndex_ = i;
                        break;
                    }
                }
            }
            games_ = GameScanner::scan(platforms_[platformIndex_]);
            platforms_[platformIndex_].gameCount = static_cast<int>(games_.size());
        }
        if (screenName == "games") {
            screen_ = Screen::Games;
            focus_ = 0;
            scroll_ = 0;
        } else {
            screen_ = Screen::Detail;
            gameIndex_ = 0;
            focus_ = 0;
            scroll_ = 0;
            const std::string deletePreview = util::getEnv("NASCADE_SCREENSHOT_DELETE");
            if (!games_.empty() && (deletePreview == "1" || deletePreview == "2")) {
                deleteConfirm_ = true;
                deleteTargetIndex_ = 0;
                deleteFocus_ = deletePreview == "2" ? 0 : 1;
            }
        }
    } else if (screenName == "settings") {
        screen_ = Screen::Settings;
        focus_ = 0;
    } else if (screenName == "about") {
        screen_ = Screen::About;
        returnScreen_ = Screen::Home;
    } else if (screenName == "smb") {
        screen_ = Screen::Smb;
        returnScreen_ = Screen::Home;
        focus_ = 0;
    } else if (screenName == "scraper") {
        screen_ = Screen::Scraper;
        returnScreen_ = Screen::Home;
        focus_ = 0;
        if (scraperStatus_.empty()) {
            if (config_.scraper.provider == "libretro") {
                scraperStatus_ = config_.scraper.baseUrl.empty() ? tr("scraper.notConfigured")
                                                                : tr("home.configured");
            } else {
                scraperStatus_ = config_.scraper.devId.empty() ? tr("scraper.notConfigured")
                                                               : tr("home.configured");
            }
        }
    } else if (screenName == "logs") {
        screen_ = Screen::Logs;
        returnScreen_ = Screen::Home;
        focus_ = 0;
    } else if (screenName == "home-connected" || screenName == "home-downloading") {
        screen_ = Screen::Home;
        focus_ = HomeConnect;
        if (screenName == "home-connected") {
            const std::string localRoot = localNasRootFromEnv();
            std::string connectError;
            if (!localRoot.empty()) {
                remoteVfs_ = makeVfs(effectiveSmbProfile(), localRoot, connectError);
                if (remoteVfs_) {
                    remoteLabel_ = tr("remote.localMock") + ": " + localRoot;
                }
            }
        } else {
            downloadRunning_ = true;
            downloadGameName_ = "Contra 4";
            downloadCoverPath_ = "/mnt/SDCARD/Imgs/NDS/1662 - Contra 4 (USA).png";
            downloadTotal_ = 33554432;
            downloadDone_->store(12000000);
            downloadTargetPath_ = "/mnt/SDCARD/Roms/NDS/Contra 4.nds";
        }
    } else if (screenName == "remote") {
        screen_ = Screen::Remote;
        focus_ = 0;
        scroll_ = 0;
        const std::string localRoot = localNasRootFromEnv();
        if (!localRoot.empty()) {
            std::string connectError;
            remoteVfs_ = makeVfs(effectiveSmbProfile(), localRoot, connectError);
            if (remoteVfs_) {
                remoteLabel_ = tr("remote.localMock") + ": " + localRoot;
                loadRemoteDir(util::getEnv("NASCADE_REMOTE_DIR"));
            } else {
                remoteError_ = connectError;
            }
        } else {
            remoteError_ = "截图模式：请设置 NASCADE_LOCAL_NAS 或使用真机连接 SMB";
        }
    } else {
        screen_ = Screen::Home;
        focus_ = 0;
    }

    drawScene();
    return saveScreenshotPpm(path, error);
}

bool App::saveScreenshotPpm(const std::string& path, std::string& error) {
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer_, &width, &height) != 0 || width <= 0 || height <= 0) {
        error = std::string("SDL_GetRendererOutputSize failed: ") + SDL_GetError();
        return false;
    }

    std::vector<Uint32> pixels(static_cast<size_t>(width) * height);
    if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGBA32,
                             pixels.data(), width * static_cast<int>(sizeof(Uint32))) != 0) {
        error = std::string("SDL_RenderReadPixels failed: ") + SDL_GetError();
        return false;
    }

    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    if (!format) {
        error = "SDL_AllocFormat failed";
        return false;
    }

    util::ensureDir(util::parentPath(path));
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        SDL_FreeFormat(format);
        error = "无法写入截图: " + path;
        return false;
    }

    out << "P6\n" << width << " " << height << "\n255\n";
    std::vector<unsigned char> row(static_cast<size_t>(width) * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            SDL_GetRGB(pixels[static_cast<size_t>(y) * width + x], format, &r, &g, &b);
            row[static_cast<size_t>(x) * 3 + 0] = r;
            row[static_cast<size_t>(x) * 3 + 1] = g;
            row[static_cast<size_t>(x) * 3 + 2] = b;
        }
        out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
    }
    SDL_FreeFormat(format);

    if (!out.good()) {
        error = "写入截图失败: " + path;
        return false;
    }
    return true;
}

}  // namespace nascade
