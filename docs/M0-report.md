# Nascade M0 技术验证报告

- 日期：2026-10-24
- 目标设备：TrimUI Brick Pro（原厂 TrimUI OS 1.1.1，aarch64）
- 开发环境：Windows 11 + 便携 MSYS2（桌面版）+ WSL1 Ubuntu 22.04 + 官方 TrimUI SDK
- 结论：桌面端与 aarch64 目标包核心链路已打通；真机已能运行，真实 SMB 连接与列目录已验证。
- 说明：本机 BIOS 未开启 VT-x，WSL2 无法启动；交叉编译使用 WSL1 完成，不影响产物。

## 已通过

### 1. SDL2 桌面 UI

- 1024×768 窗口，中文字体（Noto Sans CJK SC），手柄/键盘导航。
- 页面：首页、平台、本机游戏、游戏详情、NAS 浏览、设置、SMB、在线刮削、日志、关于。
- 截图通过 `--screenshot <file.ppm> --screen <page>` 生成，`tools/ppm_to_png.py` 转 PNG。

### 2. 原厂平台扫描

- 读取 `devdata/Emus/*/config.json`，正确解析 `label`、`rompath`、`imgpath`、`extlist`。
- 生成 5 个平台（FC/SFC/GBA/MD/PS），路径与游戏数量正确。

### 3. ScreenScraper 客户端

使用本地模拟服务 `tools/mock_screenscraper.py` 验证：

- `--test-scraper`：读取到 3 个平台。
- `--test-scrape`：解析标题、简介、开发商、发行商、发售日、类型、人数、评分、封面 URL。
- `--test-ui-scrape`：应用内异步刮削，标题/简介更新，封面下载，生成约 300px PNG 写入 `Imgs/GBA/`。
- 真实 HTTPS 走 libcurl + 内置 CA；真实凭据下还需验证配额和中文覆盖率。

### 3b. libretro 封面刮削（默认数据源）

- 数据源：`https://thumbnails.libretro.com/<平台>/Named_Boxarts/`，抓取目录列表并缓存到 `.Nascade/cache/libretro/`。
- 平台映射：FC/SFC/GB/GBC/GBA/NDS/N64/MD/SMS/GG/DC/SS/PS/PSP/PCE/WSC/NGP/MAME/FBNeo 等。
- 标题匹配：GB/GBC/GBA/SFC/MD/N64 从 zip 内第一个 ROM 或裸 ROM 读取内部标题；去标签、归一化、包含/前缀/模糊/别名匹配，最后回退文件名；按相似度、区域偏好、非零售版本排序。
- 真机 `--test-scraper`：返回“连接成功，libretro 缩略图可访问”。
- 真机 GBA 列表缓存：5961 个封面条目。
- 真机 GBA `CT特种部队1.zip`：读取 zip 内 `.gba` 内部标题 `CT SPECIAL F`，匹配到 `CT Special Forces (USA) ...png`。
- 真机临时 SD 根执行 `--test-ui-scrape`：下载封面并写出 `Imgs/GBA/CT特种部队1.png`（约 232KB，300px PNG），端到端通过。
- 限制：libretro 只有图片，不提供简介/开发商/发售日；中文文件名且无可读内部标题时可能匹配不到；街机短名 ROM 与 libretro 全名封面通常对不上。

### 4. 远程浏览 + 下载 + 续传

使用 `NASCADE_LOCAL_NAS` 指向本地目录模拟 NAS：

- `--test-ui-download`：进入 `GBA` 目录，选择 `NAS Test Game.gba`，下载到 `Roms/GBA/`，SHA1 与源文件一致。
- `--test-download`：直接下载到副本路径，SHA1 一致。
- 手工构造 64KB `.part` 后续传 128KB 文件，最终 SHA1 一致。
- 这一路径复用应用内 `startDownload()` / `Downloader` 代码；真实 SMB 只需替换 VFS 后端。

### 5. 原厂打包模板

- 参考原厂 1.1.1 的 BrickTools 包：`config.json` 使用 `label.ch.lang`、`icon: ""`、`icontop`、`launch`。
- `packaging/stockos/Nascade.pak/`：`config.json`、`config.example.json`、`launch.sh`、300×300 `icon.png`。
- `scripts/package_stockos.sh` 组装 `build/Nascade.pak.zip`；`dist/Nascade.pak.zip` 是当前可安装包（约 17MB）。
- 目录布局按原厂 `Apps/`、`Roms/`、`Imgs/`、`Emus/` 约定设计。

### 6. aarch64 交叉编译与目标验证

工具链/依赖：

- 官方 SDK 20231018：Linaro GCC 7.5.0 + `SDK_usr_tg5040_a133p` + SDL2 2.26.1
- 交叉编译 libcurl 8.11.1（静态，OpenSSL 1.1）
- 交叉编译 libsmb2（静态）
- SQLite 使用 SDK 的静态库

结果：

- 目标二进制：aarch64 ELF，3.8MB（未 strip）
- 运行库：包内 `lib/` 包含 SDL2、SDL2_ttf、freetype、zlib、bz2、libstdc++、libgcc_s
- qemu-aarch64 验证通过：`--help`、`--test-download`（SHA1 与源一致）、`--test-scraper`、`--test-scrape`（libretro 与 ScreenScraper 分支）
- qemu + SDL `offscreen` 渲染 1024×768 首页成功，包内 Noto 字体与 SDL2/SDL2_ttf 正常（`docs/images/target_home.png`）
- 安装包：本地构建 `dist/Nascade.pak.zip`（17,022,868 字节），SHA256 `aea9be228c512e85b71cb7841be7ba40766b539e41eeb296d12fb54b9bcaec9d`（构建产物不入库）
- Brick Pro 物理 A/B、X/Y 与 SDL 逻辑按键相反；控制器映射已交换（物理 A = 确认，物理 B = 返回，物理 X = 选项，物理 Y = Alt）。
- glibc 最高要求 `GLIBC_2.25`（`getrandom`）；原厂 BrickTools 二进制最高要求 2.17，真机需确认原厂 libc ≥2.25

WSL2 状态：`HyperVRequirementVirtualizationFirmwareEnabled=False`，因此 WSL2 无法启动。WSL1 已足够运行 x86_64 交叉编译器。

### 7. 原厂 1.1.1 真机验证（SSH）

设备：`<brick-ip>`（测试环境），原厂 1.1.1，内核 TinaLinux 4.9.191 aarch64。

已确认：

- glibc 2.33，`/lib/ld-linux-aarch64.so.1 -> ld-2.33.so`
- 系统 SDL2 为 2.30.8，SDL2_ttf 为 2.14.1，位于 `/usr/trimui/lib`
- `/tmp` 是 487MB tmpfs；SD 卡 58.2GB，安装前剩余 436MB
- 包内二进制在真机上 `--help` 正常（包内 SDL2 和原厂 SDL2 两种库顺序都通过）
- 真机本地下载测试 64KB，MD5 与源文件一致
- 真机 SDL offscreen 渲染首页成功，扫描到 38 个平台、3972 个游戏
- 已安装到 `/mnt/SDCARD/Apps/Nascade.pak/`；菜单标签“Nascade”需要在 `show.json` 中保留同名条目；安装后 SD 剩余约 525MB
- 真实 Samba 共享 `//<nas-ip>/<share>`：SMB2/3 连接成功，应用 `--test-smb` 在 `/Roms` 下列出 1 个文件（约 158MB 的 `.bin`）
- 修复 libsmb2 路径问题：共享内路径不能带前导 `/`（`/Roms` → `Roms`），否则 Samba 返回 `STATUS_INVALID_PARAMETER`
- 用户实机使用确认：Apps 菜单可显示并启动，SDL 全屏、中文字体、手柄导航正常；物理 A/B 和首页焦点问题已修正并部署
- libretro 封面源真机可访问；GBA 目录缓存 5961 条；`CT特种部队1.zip` 内部标题匹配成功；临时 SD 根端到端写出 300px 封面
- 设备 `.Nascade/config.json` 已切换为 `provider=libretro`、`baseUrl=https://thumbnails.libretro.com`
- 修复 UI 字体底部截断：`Painter::textIn` 的垂直裁剪扩展到完整字形高度，水平裁剪保持不变；首页、平台、详情和设置页已截图复核
- NAS 下载完成后详情页显示“已安装”徽标和按钮，隐藏进度条，状态改为“已安装到 …”；100% 但后台仍在收尾时显示“正在完成安装…”
- 首页状态卡片按状态切换：未连接显示连接提示；已连接且下载中显示游戏名/进度条/大小/百分比；已连接空闲只显示“本地游戏库”和平台/游戏统计；三种状态已离屏截图复核
- SMB 配置完整时首页直接显示本地库统计，并在启动时后台预连接；连接成功后徽标和顶栏状态切换为“已连接”，失败时静默保留“已配置”状态
- 首页 X 键改为“连接游戏库”，打开 SMB 连接页；底部提示同步更新。已连接状态下聚焦顶部卡片按 A 不再跳转 SMB，未连接时仍作为连接入口
- 长按上下键快速滚动：Input 记录按住方向，350ms 后开始重复，80ms/次，持续 1.2s 后加速到 40ms/次；应用于首页网格、平台、游戏和 NAS 列表
- 本地游戏识别修正：递归扫描子目录；补充 7z/nds/p8/iso 等后缀；FC/SFC 无后缀 ROM；按目录去重 cue/bin；跳过 db/srm/文档。真机验证 FC 8511、SFC 765、MD 804、GB 82、WSC 186、ATARI2600 688、NDS 2、PICO8 103、DC 14、PPSSPP 8 个游戏
- 本地游戏删除：详情页增加红色“删除游戏”按钮和确认框；方向键选择“删除游戏/取消”，A 执行当前选项，B 直接关闭；默认选中“取消”。删除主文件及同目录同名的 cue/bin 附属文件，更新列表/平台计数并提示 Refresh Roms；两种选中状态已离屏截图复核
- 按钮文字垂直居中：按实际字体纹理高度计算，详情、设置、刮削、SMB、日志和确认框按钮已截图复核
- NAS 连接失败页：底部显示“A 重试 / B 返回”；A/X 会释放旧 VFS 并重新连接，重试期间显示“正在连接 NAS…”；错误页已截图复核
- 首页下载卡片左侧展示游戏封面：开始下载时捕获当前封面路径，下载中完成同游戏刮削会更新；无封面时显示首字占位；下载状态卡片已截图复核
- 详情页顶栏移除游戏名副标题；平台/格式/已安装标签按实际字体高度垂直居中；详情页已截图复核
- 下载/刮削状态按游戏隔离：仅当详情页路径与下载或刮削目标一致时显示进度条、完成状态或“下载中”按钮，避免 A 下载时在 B 详情页出现
- 增加 SMB 连接健康检查：每秒检查已连接 socket，检测到对端关闭/重置后释放会话，顶栏回落到“未连接”并提示“NAS 连接已断开”；`--test-smb` 输出 `alive` 状态。顶栏状态现在按实际连接显示已连接/连接中/未连接/未配置
- 修复下载收尾卡顿：`Downloader` 先释放 `VfsReader` 关闭文件句柄，再销毁 SMB context，避免 `SmbReader` 析构使用已释放的 context；`SmbVfs::disconnect()` 不再等待可能阻塞的 `smb2_disconnect_share()`。收尾阶段新增 flush/rename/close/disconnect 日志
- 修复首页按 B 退出卡死：`App::shutdown()` 不再同步等待未完成的 SMB/刮削/下载 future，卡住的网络任务不会阻塞主线程退出。新版二进制已部署到真机，等待实测下载收尾和退出行为
- 顶栏主标题与副标题间距加大：副标题从 `y=52` 下移到 `y=61`，文字不再贴在一起；首页截图已更新
- 应用整体更名为 **Nascade**（NAS + Arcade + cascade）：界面标题、关于页、窗口标题、日志、二进制、CMake 目标、环境变量前缀、安装包目录和数据目录均已切换；旧 `.BrickVault` 数据目录会在首次启动时自动迁移到 `.Nascade`
- 真机重命名部署：新包安装到 `Apps/Nascade.pak/`，`show.json` 菜单标签改为“Nascade”（原文件已备份），旧 `BrickVault.pak` 已移除；设备数据目录已迁移到 `.Nascade`，SMB 配置保持不变

待验证：

- SMB 断点续传和断线恢复（连接、列目录、完整下载 32MB NDS 已验证）
- ScreenScraper 可选数据源的在线验证（默认 libretro 已验证）
- 退出应用返回原厂菜单后的完整回归

## 未验证 / 风险

| 项目 | 状态 | 说明 |
| --- | --- | --- |
| 真实 NAS SMB 连接 | 已验证（连接+列目录+下载） | 真机连接 Samba 共享 `//<nas-ip>/<share>`，列出 `/Roms`；目标二进制完整下载 32MB NDS 文件通过，续传/断线恢复待验证 |
| Brick Pro 原厂 Apps/SDL2 | 已验证（实机运行） | Apps 菜单启动、SDL 全屏、中文字体、手柄导航正常；A/B 与首页焦点已修正 |
| aarch64 交叉编译 | 已完成（qemu + 真机） | 官方 SDK 构建成功，真机运行正常 |
| 原厂 glibc 兼容性 | 已确认 | 原厂 1.1.1 为 glibc 2.33，满足目标二进制的 GLIBC_2.25 |
| libretro 封面刮削 | 已验证（真机+端到端） | 连接、GBA 列表缓存、内部标题匹配、封面下载和 `Imgs` 写入均通过 |
| UI 内 SMB 凭据编辑 | 未完成 | 目前通过 `.Nascade/config.json` 或环境变量 |
| 多文件游戏/暂停/队列 | 未完成 | 当前只处理单文件；cue/bin/m3u 整组下载待实现 |
| 元数据持久化 | 未完成 | libretro 只有封面；ScreenScraper 标题/简介只在当前会话 |
| 批量刮削/配额 | 未完成 | 当前为单游戏刮削 |
| 原厂菜单刷新 | 未完成 | 需要提示 `Menu → Refresh Roms`，或验证缓存删除方式 |

## 已知环境说明

- MinGW 下 libsmb2 需要两个本地补丁：定义 `_WINDOWS`、绕过 `asprintf` 冲突。这些补丁只用于 Windows 桌面验证，**不要应用到 Linux/aarch64 目标构建**。
- Windows 桌面应用使用 Noto 字体；真机包必须把字体放到 `assets/fonts/`。
- 应用数据在 `/mnt/SDCARD/.Nascade/`；原厂包在 `/mnt/SDCARD/Apps/Nascade.pak/`。

## 下一步

1. （已完成）真机 Apps 菜单、SDL2 显示、手柄、字体验证；继续回归退出行为。
2. 真实 NAS 连接与列目录已完成；继续测试下载、续传和断线恢复，配置写入 `.Nascade/config.json`。
3. 补齐 UI 凭据编辑（屏幕键盘）和多文件游戏安装。
4. 增加 SQLite 元数据缓存、批量封面刮削和匹配失败时的候选选择。
5. 安装完成后提示/触发原厂游戏库刷新。
