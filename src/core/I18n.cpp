#include "core/I18n.h"

#include <unordered_map>
#include <utility>

namespace nascade {

namespace {

using Pair = std::pair<const char*, const char*>;

const std::unordered_map<std::string, Pair>& strings() {
    static const std::unordered_map<std::string, Pair> table = {
        {"app.name", {"Nascade", "Nascade"}},
        {"app.tagline", {"NAS 游戏库", "NAS Game Library"}},
        {"app.version", {"0.1.0 · M0", "0.1.0 · M0"}},

        {"home.heroTitle", {"连接你的 NAS", "Connect your NAS"}},
        {"home.heroDesc", {"通过 SMB2/3 读取游戏目录，只读访问，不改动 NAS 文件。", "Browse your game shares over SMB2/3. Read-only."}},
        {"home.status", {"状态", "Status"}},
        {"home.notConfigured", {"未配置", "Not configured"}},
        {"home.configured", {"已配置", "Configured"}},
        {"home.notConnected", {"未连接", "Not connected"}},
        {"home.connected", {"已连接", "Connected"}},
        {"home.connectedTo", {"已连接", "Connected to"}},
        {"home.connecting", {"连接中", "Connecting"}},
        {"home.connectingTo", {"正在连接", "Connecting to"}},
        {"home.configuredTo", {"SMB 已配置", "SMB configured"}},
        {"home.downloading", {"下载中", "Downloading"}},
        {"home.downloadingTitle", {"正在下载游戏", "Downloading game"}},
        {"home.libraryTitle", {"本地游戏库", "Local library"}},
        {"home.libraryHint", {"按 A 管理 SMB 连接", "Press A to manage SMB"}},
        {"home.platforms", {"个平台", "platforms"}},
        {"home.games", {"个游戏", "games"}},
        {"home.nas", {"NAS 游戏", "NAS games"}},
        {"home.nasDesc", {"SMB 浏览与下载", "Browse and download over SMB"}},
        {"home.local", {"本机游戏", "Local games"}},
        {"home.localDesc", {"SD 卡上的游戏", "Games on the SD card"}},
        {"home.browse", {"浏览游戏", "Browse games"}},
        {"home.browseDesc", {"平台、清单与详情", "Platforms, lists and details"}},
        {"home.downloads", {"下载队列", "Downloads"}},
        {"home.downloadsDesc", {"传输进度与历史", "Progress and history"}},
        {"home.scrape", {"在线刮削", "Metadata"}},
        {"home.scrapeDesc", {"封面、简介与资料", "Covers, synopsis and details"}},
        {"home.scrapeDescLibretro", {"libretro 封面图片", "libretro cover images"}},
        {"home.settings", {"设置", "Settings"}},
        {"home.settingsDesc", {"语言、网络与缓存", "Language, network and cache"}},
        {"home.about", {"关于", "About"}},
        {"home.aboutDesc", {"版本与开源许可", "Version and licenses"}},
        {"home.quit", {"退出", "Quit"}},
        {"home.quitDesc", {"返回原厂 Apps 菜单", "Return to the stock Apps menu"}},

        {"nav.confirm", {"A 确认", "A Confirm"}},
        {"nav.back", {"B 返回", "B Back"}},
        {"nav.option", {"X 操作", "X Action"}},
        {"nav.connectLibrary", {"X 连接游戏库", "X Connect library"}},
        {"nav.view", {"Y 视图", "Y View"}},
        {"nav.menu", {"Start 菜单", "Start Menu"}},
        {"nav.page", {"L/R 翻页", "L/R Page"}},
        {"nav.adjust", {"←→ 修改", "←→ Adjust"}},
        {"nav.enter", {"A 进入", "A Open"}},

        {"platforms.title", {"选择平台", "Platforms"}},
        {"platforms.subtitle", {"平台来自 SD 卡 Emus 配置", "Platforms discovered from Emus configs"}},
        {"platforms.empty", {"未发现平台", "No platforms found"}},
        {"platforms.emptyHint", {"请确认 SD 卡存在 Emus 目录和 config.json。", "Make sure the SD card has an Emus directory with config.json files."}},
        {"platforms.romPath", {"ROM 目录", "ROM path"}},
        {"platforms.imgPath", {"封面目录", "Cover path"}},
        {"platforms.exts", {"扩展名", "Extensions"}},
        {"platforms.count", {"个游戏", "games"}},
        {"platforms.open", {"按 A 浏览游戏", "Press A to browse"}},
        {"platforms.refresh", {"重新扫描", "Rescan"}},
        {"platforms.fallback", {"内置兜底映射", "Built-in fallback mapping"}},

        {"games.title", {"游戏清单", "Games"}},
        {"games.empty", {"这个平台还没有游戏", "No games in this platform"}},
        {"games.installed", {"已安装", "Installed"}},
        {"games.local", {"本地", "Local"}},
        {"games.remote", {"NAS", "NAS"}},
        {"games.detail", {"按 A 查看详情", "Press A for details"}},

        {"detail.title", {"游戏详情", "Game details"}},
        {"detail.path", {"文件路径", "File path"}},
        {"detail.platform", {"平台", "Platform"}},
        {"detail.size", {"大小", "Size"}},
        {"detail.format", {"格式", "Format"}},
        {"detail.description", {"简介", "Synopsis"}},
        {"detail.noDescription", {"暂无简介；可在线刮削获取。", "No synopsis yet. Scrape online to fetch metadata."}},
        {"detail.noDescriptionLibretro", {"暂无简介；libretro 封面库只提供图片。", "No synopsis; the libretro cover library provides images only."}},
        {"detail.download", {"下载并安装", "Download & install"}},
        {"detail.localGame", {"已在本机", "On device"}},
        {"detail.delete", {"删除游戏", "Delete game"}},
        {"detail.deleteTitle", {"删除本地游戏？", "Delete local game?"}},
        {"detail.deleteWarning", {"将从 SD 卡删除该游戏文件，操作不可撤销。", "This will delete the game file from the SD card. This cannot be undone."}},
        {"detail.scrape", {"在线刮削", "Scrape metadata"}},
        {"detail.back", {"返回清单", "Back to list"}},
        {"detail.remotePath", {"远程路径", "Remote path"}},

        {"remote.title", {"NAS 游戏", "NAS games"}},
        {"remote.connecting", {"正在连接 NAS…", "Connecting to NAS…"}},
        {"remote.connected", {"已连接", "Connected"}},
        {"remote.disconnected", {"NAS 连接已断开", "NAS connection lost"}},
        {"remote.empty", {"目录为空", "Empty directory"}},
        {"remote.dir", {"目录", "Directory"}},
        {"remote.file", {"文件", "File"}},
        {"remote.unknownPlatform", {"未知平台", "Unknown platform"}},
        {"remote.localMock", {"本地模拟 NAS", "Local mock NAS"}},
        {"remote.up", {"上一级", "Up"}},
        {"remote.reload", {"X 重新加载", "X Reload"}},
        {"remote.retry", {"A 重试", "A Retry"}},

        {"download.running", {"下载中…", "Downloading…"}},
        {"download.finishing", {"正在完成安装…", "Finishing install…"}},
        {"download.done", {"下载完成", "Download complete"}},
        {"download.failed", {"下载失败", "Download failed"}},
        {"download.exists", {"目标文件已存在", "Target file already exists"}},
        {"download.noPlatform", {"未找到对应平台目录", "No matching platform folder"}},
        {"download.installed", {"已安装到", "Installed to"}},
        {"download.refreshHint", {"请返回原厂菜单执行 Refresh Roms", "Return to the stock menu and run Refresh Roms"}},

        {"settings.title", {"设置", "Settings"}},
        {"settings.language", {"语言", "Language"}},
        {"settings.fontScale", {"字体大小", "Font size"}},
        {"settings.view", {"默认视图", "Default view"}},
        {"settings.smb", {"SMB 连接", "SMB connection"}},
        {"settings.scrape", {"在线刮削", "Online metadata"}},
        {"settings.logs", {"日志", "Logs"}},
        {"settings.about", {"关于", "About"}},
        {"settings.back", {"返回首页", "Back to home"}},
        {"settings.value.zh", {"简体中文", "简体中文"}},
        {"settings.value.en", {"English", "English"}},
        {"settings.value.list", {"列表", "List"}},
        {"settings.value.grid", {"网格", "Grid"}},
        {"settings.value.small", {"小", "Small"}},
        {"settings.value.medium", {"中", "Medium"}},
        {"settings.value.large", {"大", "Large"}},
        {"settings.saved", {"设置已保存", "Settings saved"}},

        {"about.title", {"关于 Nascade", "About Nascade"}},
        {"about.version", {"版本", "Version"}},
        {"about.target", {"目标设备", "Target device"}},
        {"about.protocol", {"网络协议", "Protocol"}},
        {"about.license", {"许可证", "License"}},
        {"about.licenseText", {"开源项目；不提供、不内置任何游戏内容。", "Open source. No game content is provided or bundled."}},
        {"about.dataSource", {"元数据来源", "Metadata source"}},
        {"about.disclaimer", {"请只访问你拥有合法使用权的游戏文件。", "Only access game files you are legally entitled to use."}},

        {"smb.title", {"SMB 连接", "SMB connection"}},
        {"smb.host", {"主机", "Host"}},
        {"smb.share", {"共享名", "Share"}},
        {"smb.root", {"根目录", "Root path"}},
        {"smb.user", {"用户名", "Username"}},
        {"smb.password", {"密码", "Password"}},
        {"smb.test", {"测试连接", "Test connection"}},
        {"smb.notConfigured", {"尚未配置 SMB 连接", "SMB is not configured"}},
        {"smb.placeholder", {"测试会读取共享根目录；M0 临时方式是在 .Nascade/config.json 中填写凭据。", "The test reads the share root. For M0, put credentials in .Nascade/config.json."}},
        {"smb.testing", {"正在测试连接…", "Testing connection…"}},
        {"smb.advanced", {"高级选项", "Advanced"}},

        {"scraper.title", {"在线刮削", "Online metadata"}},
        {"scraper.provider", {"数据源", "Provider"}},
        {"scraper.enabled", {"启用在线刮削", "Enable online metadata"}},
        {"scraper.language", {"元数据语言", "Metadata language"}},
        {"scraper.credentials", {"API 凭据", "API credentials"}},
        {"scraper.notConfigured", {"未配置", "Not configured"}},
        {"scraper.configure", {"配置凭据", "Configure credentials"}},
        {"scraper.consent", {"首次使用需同意 ScreenScraper 服务条款，并确认只发送游戏名称/哈希。", "You must accept ScreenScraper's terms and allow game name/hash to be sent."}},
        {"scraper.placeholder", {"凭据配置与测试将在 M0 完成。", "Credential setup and testing will be implemented in M0."}},
        {"scraper.test", {"测试凭据", "Test credentials"}},
        {"scraper.libretroProvider", {"libretro thumbnails", "libretro thumbnails"}},
        {"scraper.libretroSubtitle", {"libretro 封面库（仅图片）", "libretro cover library (images only)"}},
        {"scraper.noCredentials", {"无需凭据", "No credentials required"}},
        {"scraper.testLibretro", {"测试封面源", "Test cover source"}},
        {"scraper.coverRegion", {"封面区域", "Cover region"}},
        {"scraper.region.us", {"美版优先", "USA first"}},
        {"scraper.region.eu", {"欧版优先", "Europe first"}},
        {"scraper.region.jp", {"日版优先", "Japan first"}},
        {"scraper.libretroHint", {"封面来源：thumbnails.libretro.com。按 ROM 内部标题/文件名匹配，只下载封面图片，不提供简介、开发商等文字信息。", "Cover source: thumbnails.libretro.com. Matches by ROM internal title/filename and downloads cover images only; no synopsis or developer text."}},
        {"scraper.status", {"状态", "Status"}},

        {"logs.title", {"日志", "Logs"}},
        {"logs.subtitle", {"导出与诊断", "Export & diagnostics"}},
        {"logs.hint", {"日志位于 .Nascade/logs，导出前请检查是否包含敏感信息。", "Logs are stored in .Nascade/logs. Check for sensitive data before exporting."}},
        {"logs.export", {"导出日志", "Export logs"}},

        {"toast.disabled", {"功能开发中", "Coming soon"}},
        {"toast.scrapePlaceholder", {"在线刮削将在 M0 接入 ScreenScraper", "ScreenScraper integration is part of M0"}},
        {"toast.smbPlaceholder", {"SMB 连接将在 M0 接入 libsmb2", "libsmb2 SMB integration is part of M0"}},
        {"toast.saved", {"已保存", "Saved"}},
        {"toast.testPending", {"测试功能尚未接入", "Test is not wired yet"}},
        {"toast.noCreds", {"请先填写 ScreenScraper 凭据", "Enter ScreenScraper credentials first"}},
        {"toast.rescanned", {"已重新扫描", "Rescanned"}},
        {"toast.exported", {"日志已导出", "Logs exported"}},
        {"toast.exportFailed", {"导出失败", "Export failed"}},
        {"toast.saveFailed", {"保存失败", "Save failed"}},
        {"toast.gridLater", {"网格视图将在后续实现", "Grid view is coming later"}},
        {"toast.scraperRunning", {"正在测试刮削源…", "Testing scraper source…"}},
        {"toast.noDevCreds", {"请先填写 devid 和 devpassword", "Enter devid and devpassword first"}},
        {"toast.localDownload", {"当前是本地演示模式；SMB 下载将在 M0 接入", "Local demo mode; SMB download is part of M0"}},
        {"toast.localGame", {"游戏已在本机，请从原厂游戏菜单启动", "This game is already on the device; launch it from the stock game menu"}},
        {"toast.deleted", {"已删除", "Deleted"}},
        {"toast.deleteFailed", {"删除失败", "Delete failed"}},

        {"scraper.testing", {"正在测试…", "Testing…"}},
        {"scraper.scraping", {"正在刮削…", "Scraping…"}},
        {"scraper.success", {"刮削完成", "Scrape complete"}},
        {"scraper.failed", {"刮削失败", "Scrape failed"}},
        {"scraper.notConfiguredHint", {"M0 临时方式：在 .Nascade/config.json 中填写凭据。", "M0 temporary: fill credentials in .Nascade/config.json."}},
        {"scraper.devOnly", {"仅 dev 凭据", "dev credentials only"}},
        {"scraper.devAndUser", {"dev + 用户凭据", "dev + user credentials"}},

        {"smb.saved", {"已保存", "Saved"}},
        {"smb.notSaved", {"未保存", "Not saved"}},
        {"smb.credentials", {"凭据", "Credentials"}},

        {"logs.path", {"日志文件", "Log file"}},
        {"logs.exportHint", {"导出到 SD 卡根目录 Nascade.log", "Export to Nascade.log on the SD card root"}},

        {"about.targetValue", {"TrimUI Brick Pro · 原厂 TrimUI OS", "TrimUI Brick Pro · Stock TrimUI OS"}},
        {"about.protocolValue", {"SMB2/3（只读）", "SMB2/3 (read-only)"}},
        {"about.licenseValue", {"开源（许可证待定）", "Open source (license TBD)"}},

        {"common.arrow", {">", ">"}},
        {"home.heroHint", {"按 A 配置 SMB 连接", "Press A to configure SMB"}},

        {"common.back", {"返回", "Back"}},
        {"common.on", {"开", "On"}},
        {"common.off", {"关", "Off"}},
        {"common.yes", {"是", "Yes"}},
        {"common.no", {"否", "No"}},
        {"common.none", {"无", "None"}},
        {"common.unknown", {"未知", "Unknown"}},
        {"common.loading", {"加载中…", "Loading…"}},
        {"common.error", {"错误", "Error"}},
        {"common.ok", {"确定", "OK"}},
        {"common.cancel", {"取消", "Cancel"}},
        {"common.quitConfirm", {"确定退出 Nascade？", "Quit Nascade?"}},
    };
    return table;
}

}  // namespace

I18n& I18n::instance() {
    static I18n i18n;
    return i18n;
}

void I18n::setLanguage(const std::string& lang) {
    lang_ = (lang == "en") ? "en" : "zh";
}

std::string I18n::t(const std::string& key) const {
    const auto& table = strings();
    const auto it = table.find(key);
    if (it == table.end()) return key;
    return lang_ == "en" ? it->second.second : it->second.first;
}

}  // namespace nascade
