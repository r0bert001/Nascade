#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "core/Config.h"
#include "core/Paths.h"
#include "model/Game.h"
#include "model/Platform.h"
#include "scrape/ScreenScraper.h"
#include "storage/Database.h"
#include "ui/FontCache.h"
#include "ui/ImageCache.h"
#include "ui/Input.h"
#include "ui/Painter.h"
#include "vfs/Vfs.h"

namespace nascade {

class App {
public:
    App();
    ~App();

    bool init(bool windowed, bool hidden, std::string& error);
    int run();
    bool renderScreenshot(const std::string& path, const std::string& screenName, std::string& error);
    bool runDownloadSelfTest(const std::string& remoteDir, const std::string& fileName,
                             std::string& error);
    bool runScrapeSelfTest(const std::string& platformId, std::string& error);
    void shutdown();

private:
    enum class Screen {
        Home, Platforms, Games, Detail, Settings, About, Smb, Scraper, Logs, Remote
    };
    enum HomeItem {
        HomeConnect = 0,
        HomeNas,
        HomeBrowse,
        HomeScrape,
        HomeSettings,
        HomeAbout,
        HomeQuit,
        HomeItemCount
    };

    bool initSdl(bool windowed, bool hidden, std::string& error);
    bool initFont(std::string& error);
    void loadData();
    void openPlatforms();
    void openGames(int platformIndex);
    void rescan();
    void saveConfig();
    void go(Screen screen, Screen returnTo);
    void setToast(const std::string& message);
    void startScraperTest();
    void pollScraper();
    void startScrape();
    void pollScrape();
    ScraperConfig effectiveScraperConfig() const;
    SmbProfile effectiveSmbProfile() const;
    void startSmbTest();
    void pollSmb();

    void openRemote();
    void startRemoteConnect(bool showScreen);
    void pollRemoteConnect();
    void pollRemoteHealth();
    void loadRemoteDir(const std::string& path);
    void openRemoteFile(int index);
    std::string remotePlatformFor(const std::string& fileName) const;
    std::string remoteParent(const std::string& path) const;
    std::string joinRemote(const std::string& dir, const std::string& name) const;
    void startDownload();
    void pollDownload();
    void confirmDeleteGame();

    void handle(Action action);
    void handleHome(Action action);
    void handlePlatforms(Action action);
    void handleGames(Action action);
    void handleDetail(Action action);
    void handleSettings(Action action);
    void handleAbout(Action action);
    void handleSmb(Action action);
    void handleScraper(Action action);
    void handleLogs(Action action);
    void handleRemote(Action action);

    void draw();
    void drawScene();
    void drawTopBar(const std::string& title, const std::string& subtitle);
    void drawBottomBar(const std::vector<std::string>& hints);
    void drawHome();
    void drawPlatforms();
    void drawGames();
    void drawDetail();
    void drawSettings();
    void drawAbout();
    void drawSmb();
    void drawScraper();
    void drawLogs();
    void drawRemote();
    void drawDeleteConfirm();
    void drawToast();

    void drawRow(int x, int y, int w, int h, bool focused, const std::string& title,
                 const std::string& subtitle, const std::string& right, int seed);
    void drawButton(int x, int y, int w, int h, bool focused, const std::string& label,
                    bool primary, bool danger = false);
    int fontSize(int base) const;
    int visibleRows(int contentHeight, int rowHeight) const;
    void clampFocus(int count);
    void ensureVisible(int count, int visible, int rowHeight);

    bool saveScreenshotPpm(const std::string& path, std::string& error);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    FontCache fonts_;
    ImageCache images_;
    std::unique_ptr<Painter> painter_;
    Input input_;
    Paths paths_;
    AppConfig config_;
    Database db_;

    std::vector<Platform> platforms_;
    std::vector<Game> games_;

    Screen screen_ = Screen::Home;
    Screen returnScreen_ = Screen::Home;
    int focus_ = 0;
    int scroll_ = 0;
    int platformIndex_ = 0;
    int gameIndex_ = 0;
    bool running_ = true;
    bool screenshotMode_ = false;
    bool shutdownDone_ = false;

    std::string toast_;
    uint32_t toastUntil_ = 0;
    std::string scraperStatus_;
    std::future<ScraperTestResult> scraperFuture_;
    bool scraperRunning_ = false;

    struct ScrapeOutcome {
        bool ok = false;
        ScrapedGame data;
        std::string error;
        std::string coverPath;
    };
    std::string scrapeStatus_;
    std::future<ScrapeOutcome> scrapeFuture_;
    bool scrapeRunning_ = false;
    int scrapeGameIndex_ = -1;
    std::string scrapeGamePath_;

    std::string smbStatus_;
    std::future<std::string> smbFuture_;
    bool smbRunning_ = false;

    struct RemoteConnectOutcome {
        bool ok = false;
        std::string error;
        std::unique_ptr<Vfs> vfs;
        std::string label;
    };
    std::unique_ptr<Vfs> remoteVfs_;
    std::string remoteLabel_;
    std::string remoteCwd_;
    std::vector<VfsEntry> remoteEntries_;
    std::string remoteError_;
    std::future<RemoteConnectOutcome> remoteConnectFuture_;
    bool remoteConnecting_ = false;
    bool remoteConnectSilent_ = false;
    uint32_t remoteHealthCheckAt_ = 0;
    bool remoteMode_ = false;
    bool deleteConfirm_ = false;
    int deleteTargetIndex_ = -1;
    int deleteFocus_ = 1;
    int remoteSelectedIndex_ = -1;

    struct DownloadOutcome {
        bool ok = false;
        std::string error;
    };
    std::future<DownloadOutcome> downloadFuture_;
    std::shared_ptr<std::atomic<uint64_t>> downloadDone_ =
        std::make_shared<std::atomic<uint64_t>>(0);
    uint64_t downloadTotal_ = 0;
    bool downloadRunning_ = false;
    std::string downloadStatus_;
    std::string downloadGameName_;
    std::string downloadCoverPath_;
    std::string downloadRemotePath_;
    std::string downloadTargetPath_;
};

}  // namespace nascade
