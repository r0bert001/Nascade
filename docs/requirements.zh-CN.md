# TrimUI Brick Pro 原厂固件游戏商店需求清单

> 文档版本：v0.2（范围已确认）  
> 日期：2026-10-23  
> 状态：范围已确认，进入 M0 技术验证  
> M0 进展（2026-10-24）：桌面 SDL2 UI、Emus 平台扫描、SMB 后端、libretro 封面刮削、ScreenScraper 可选元数据、远程浏览/下载/续传、封面写入 `Imgs` 已实现；原厂 aarch64 构建、真机 SMB 和 libretro 端到端已验证。
> 目标设备：TrimUI Brick Pro  
> 目标系统：原厂 TrimUI OS 1.1.1（Stock OS，aarch64）  
> 本期协议：SMB2/3  
> 项目名：Nascade（NAS + Arcade + cascade）

## 0. 已确认的产品决策

| 项目 | 决策 |
| --- | --- |
| 目标固件 | 只做原厂 TrimUI OS；NextUI、CrossMix、Knulli 不在本期 |
| 协议 | 只做 SMB2/3；NFS、FTP 不在本期 |
| 功能范围 | 浏览 NAS 游戏目录、查看清单/详情、下载并安装到 SD 卡 |
| 元数据 | 封面默认 libretro-thumbnails；文字元数据可选 ScreenScraper；本地元数据兜底 |
| 技术栈 | C++17 + SDL2 |
| 内容来源 | 扫描 NAS 游戏目录 + 可选 `store.json` |
| 安装策略 | 默认原样复制；按平台可选解压 |
| 语言 | 简体中文默认 + 英文 |
| 发布方式 | GitHub 开源发布；原厂 `Apps` 手动安装 |
| 项目名 | Nascade（NAS + Arcade + cascade） |

## 1. 产品定位与边界

### 1.1 一句话定位

Nascade 是运行在 TrimUI Brick Pro 原厂固件上的**私有 NAS 游戏库客户端**：通过 SMB 连接家中 NAS，浏览游戏目录与清单，在线刮削封面/简介，把游戏下载并安装到 SD 卡的原厂 `Roms` 目录，再由原厂游戏菜单启动。

它不是公共游戏商店，不提供、不索引、不分发任何游戏内容。

### 1.2 核心链路

`配置 SMB 连接 → 浏览 NAS 游戏目录 → 查看清单/详情 → 在线刮削元数据 → 下载到 SD 卡 → 写入 Roms/Imgs → Refresh Roms → 原厂游戏菜单启动`

### 1.3 本期做

- SMB2/3 连接管理与浏览。
- 游戏识别、平台归类、清单、详情、搜索、排序。
- 在线刮削：标题、简介、封面、截图、类型、开发商、发行商、发售日、人数、评分。
- 下载、暂停/继续、断点续传、队列、冲突处理、校验（有哈希时）。
- 按原厂 `Emus/*/config.json` 自动发现平台与目标目录。
- 封面写入原厂 `Imgs/<SYSTEM>/`，供原厂游戏菜单显示。
- 已安装游戏、空间管理、更新检测（P1）。
- 全手柄、简体中文、1024×768、离线缓存。

### 1.4 本期不做

- NextUI/CrossMix/Knulli 适配（后续可评估）。
- NFS、FTP、WebDAV、SMB1、NFS Kerberos。
- 公共商店、支付、账号、评论、推荐算法。
- 内置模拟器、BIOS 或游戏 ROM。
- 从 NAS 流式运行游戏。
- 手机/PC 客户端、云存档同步（P2 评估）。
- 触摸操作。

## 2. 原厂固件运行环境

### 2.1 SD 卡目录结构

原厂 TrimUI OS 的 SD 根目录通常包含：

| 目录 | 用途 |
| --- | --- |
| `Apps/` | 应用；每个应用一个文件夹，含 `config.json` 和 `launch.sh` |
| `Emus/` | 模拟器；每个平台一个文件夹，含 `config.json` 和 `launch.sh` |
| `Roms/` | ROM；按平台短标签分目录，如 `Roms/FC`、`Roms/GBA` |
| `Imgs/` | 封面；按平台分目录，如 `Imgs/GBA/<romname>.png` |
| `RetroArch/` | RetroArch 配置、核心、BIOS 等 |
| `_theme/` | 原厂 UI 主题资源 |
| `Apps/Nascade.pak/` | 本项目应用目录（计划） |

> Brick Pro 的原厂基础包可能比 Brick TG3040 更新；目录结构大概率一致，但必须在 M0 用真机确认。

### 2.2 应用打包与启动

原厂应用目录示例：

```text
Apps/
└── Nascade.pak/
    ├── config.json
    ├── icon.png
    ├── launch.sh
    ├── nascade
    ├── lib/               # 需要随包的 .so（如有）
    └── assets/            # 字体、CA 证书等
```

`config.json` 最小示例：

```json
{
  "label": "Nascade",
  "icon": "icon.png",
  "iconsel": "icon.png",
  "icontop": "icon.png",
  "launch": "launch.sh",
  "description": "NAS 游戏库"
}
```

说明：

- `icon`/`iconsel` 是旧版字段；`icontop` 是 v1.0.5 之后新增的顶层图标字段。同时提供可兼容不同固件版本。
- 图标尺寸、透明通道和选中态样式需在 M0 按真机效果调整。
- 应用放入 `Apps/` 后通常需要重启，才会出现在 Apps 菜单；M0 验证是否有刷新方式。

`launch.sh` 示例：

```sh
#!/bin/sh
cd "$(dirname "$0")"
export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
./nascade > "$PWD/launch.log" 2>&1
```

- 应用退出后应返回原厂 Apps 菜单，不重启设备。
- 日志先写 pak 目录，运行稳定后可改写到 SD 根目录的 `.Nascade/logs/`。
- 不依赖 root，不修改系统分区。

### 2.3 平台发现：读取 `Emus/*/config.json`

原厂模拟器配置示例（`Emus/GBA/config.json`）：

```json
{
  "label": "GBA",
  "rompath": "../../Roms/GBA",
  "imgpath": "../../Imgs/GBA",
  "extlist": "gba|zip",
  "launch": "launch.sh"
}
```

应用启动时扫描 `Emus/*/config.json`，解析：

- `label`：显示名。
- `rompath`：游戏安装目标目录。
- `imgpath`：封面目标目录。
- `extlist`：该平台支持的扩展名。
- `launch`：模拟器启动脚本（本期不直接调用）。

**这是本项目最重要的原厂集成点**：不硬编码平台目录，而是以用户 SD 卡上实际安装的模拟器配置为准。这样 Brick Pro 的不同固件版本、不同模拟器组合都能适配。

若 `Emus` 为空或配置缺失，使用内置兜底映射表（见 7.3），并允许用户手动指定目录。

### 2.4 `Roms`、`Imgs` 与刷新机制

- ROM 放在 `Roms/<SYSTEM>/`，`SYSTEM` 是短标签，如 `FC`、`SFC`、`GB`、`GBC`、`GBA`、`MD`、`PS`、`N64`、`PCE`、`ARCADE`、`PSP`、`DC`。
- 封面放在 `Imgs/<SYSTEM>/<romname>.png`，文件名必须与 ROM 主文件名一致，格式为 PNG。
- 原厂游戏菜单添加 ROM 后，需要执行 `Menu → Refresh Roms`（或重启）才刷新。
- 每个平台目录可能有 `<SYSTEM>_cache7.db` 缓存文件；官方手册说明可删除该文件强制重建。M0 验证在应用运行时删除是否安全；MVP 默认只提示用户刷新，不自动删除。
- 封面写入后，可能需要刷新/重启才能在原厂菜单显示；MVP 在安装完成后提示。

### 2.5 运行环境与工具链

| 项目 | 结论 | 影响 |
| --- | --- | --- |
| 架构 | aarch64（A133P Cortex-A53） | 需要 aarch64 工具链；M0 用真机确认 |
| 官方 SDK | `trimui/toolchain_sdk_smartpro` 20231018 | 含 Linaro GCC 7.5.0、`SDK_usr_tg5040_a133p`、`SDL2-2.26.1.GE8300` |
| SDL2 | 原厂系统提供 SDL2/SDL2_ttf；LedControl 已验证 | 优先链接系统库；必要时随包带 `.so` |
| 图像解码 | 不依赖 SDL2_image | 使用 stb_image，减少依赖 |
| 屏幕 | 1024×768，无触摸 | 手柄导航；安全边距；4:3 |
| 权限 | 普通用户，无 root | SMB 走用户态库；不做内核挂载 |
| 存储 | microSD，常见 FAT32 | 单文件 4GB 限制；需检测并提示 |
| 网络 | Wi-Fi 802.11 b/g/n | 实际吞吐有限；必须续传；默认单任务 |

**构建策略：**

- 目标构建使用官方 SDK（Linaro GCC 7.5.0 + sysroot），在 Docker/CI 中完成，保证与 Stock OS 的 glibc/ABI 匹配。
- 本地 Windows 没有 g++/cmake/docker。桌面调试构建需要安装便携 MSYS2 或使用 Linux/WSL；目标构建通过 CI 或 Docker。
- GCC 7.5 对 C++17 支持有限，避免依赖 `std::filesystem` 的新特性；需要时链接 `-lstdc++fs` 或使用 POSIX 封装。

### 2.6 M0 必须确认的原厂细节

1. Brick Pro 的 Stock OS 是否仍使用 `Apps/`、`Emus/`、`Roms/`、`Imgs/` 结构。
2. `config.json` 的图标字段、尺寸、选中态字段。
3. Stock OS 的 SDL2/SDL2_ttf 版本，以及应用能否直接链接系统库。
4. 应用是否需要特殊退出码或信号才能返回 Apps 菜单。
5. `Menu → Refresh Roms` 的触发方式；删除 `*_cache7.db` 是否安全。
6. `Imgs` 封面的尺寸、格式、命名和刷新行为。
7. SMB 下载到 SD 的路径与权限；FAT32/exFAT 支持。
8. 休眠/唤醒时应用的行为和信号。

## 3. 在线刮削元数据方案

### 3.1 目标

- 为 NAS 上的游戏自动获取标题、简介、封面、截图、类型、开发商、发行商、发售日、人数、评分。
- 默认请求简体中文（`zh`），缺失时回退英文（`en`）。
- 元数据缓存到本地，离线也能查看；避免重复请求。
- 支持单游戏手动刮削（P0）和整库批量刮削（P1）。
- 封面可写入原厂 `Imgs/<SYSTEM>/<romname>.png`，让原厂游戏菜单也能显示。

### 3.2 默认封面源：libretro-thumbnails

- 站点：`https://thumbnails.libretro.com/`
- 目录：`<平台>/Named_Boxarts/`，同时保留未来使用 `Named_Snaps`、`Named_Titles` 的扩展点。
- 平台映射：FC/SFC/GB/GBC/GBA/NDS/N64/MD/SMS/GG/DC/SS/PS/PSP/PCE/WSC/NGP/MAME/FBNeo 等。
- 匹配：GB/GBC/GBA/SFC/MD/N64 读取 ROM（含 zip 内第一个 ROM）的内部标题；去标签、归一化、包含/前缀/模糊/别名匹配，最后回退文件名；按相似度、区域偏好、非零售版本排序。
- 缓存：封面文件名列表缓存到 `.Nascade/cache/libretro/`，默认 7 天。
- 限制：只有图片，没有简介/开发商/发售日；中文文件名且无可读内部标题时可能匹配不到；街机短名 ROM 与全名封面通常对不上。

### 3.3 可选文字元数据源：ScreenScraper

- API：`https://api.screenscraper.fr/api2/`
- 主要接口：
  - `jeuInfos.php`：按 gameid、哈希或文件名获取游戏信息与媒体。
  - `jeuRecherche.php`：按名称搜索。
  - `systemeListe.php`：获取平台与 `systemeid`。
- 必要参数：`devid`、`devpassword`、`softname`、`output=json`。
- 用户参数：`ssid`、`sspassword`；未登录时配额很低，强烈建议使用账号。
- 语言参数：`langue=zh`；中文资料覆盖率有限，缺失时回退 `en`。
- 匹配参数：`romnom`、`romtaille`、`crc`、`md5`、`sha1`、`systemeid`、`gameid`、`romtype`。
- 返回媒体：封面（box-2D）、截图、wheel/logo、视频等；本期只下载封面和截图。
- 速率与配额：响应中包含用户配额信息；必须串行请求、限速、缓存，遇到配额超限停止并提示。
- 开发者凭据：需要 ScreenScraper 开发者账号的 `devid/devpassword`，由用户自行申请并填写；应用不内置、不公开任何凭据。
- 使用条款：遵守 ScreenScraper 的使用条款；应用内展示数据来源；媒体仅用于个人库缓存，不再分发。

### 3.4 匹配策略（ScreenScraper）

1. 计算 ROM 的 CRC32/MD5/SHA1 和大小，优先按哈希匹配。
2. ZIP 压缩包：读取包内主 ROM 文件后计算哈希；7z/RAR 后续支持。
3. `.cue`+`.bin`：以 `.cue` 为主文件匹配；多碟以 `.m3u` 或首碟为主。
4. 哈希失败时，用 ROM 文件名（去扩展名、清理版本标记）+ 平台 `systemeid` 搜索。
5. 仍失败时，提供手动搜索：输入名称或 ScreenScraper gameid，选择匹配项并缓存。
6. 平台映射：优先使用 `Emus` 的短标签映射到 ScreenScraper 的 `systemeid`；映射表可配置。

### 3.5 媒体与缓存

- 元数据存 SQLite；原始 JSON 可压缩存储，便于排查。
- 封面/截图下载到 `/mnt/SDCARD/.Nascade/cache/media/`，按内容哈希命名，避免重名。
- 应用内使用的封面生成缩略图（如 256×256），详情页使用较大图（如 512px）。
- 写入原厂 `Imgs` 时统一转成 PNG，默认宽度约 300px（Brick 社区推荐值），保留比例。
- 缓存 TTL 默认 30 天；详情页提供“重新刮削”；批量刮削可强制刷新。
- 配额/网络失败不覆盖已有元数据；保留上次成功结果。

### 3.6 Provider 抽象与后续数据源

当前已实现 libretro-thumbnails（封面）和 ScreenScraper（文字元数据）。后续可增加：

- TheGamesDB：需要 API key，英文为主。
- IGDB：需要 Twitch OAuth，英文为主。
- GameTDB：封面/多语言标题，适合任天堂平台。
- Bangumi：中文标题/简介补充源，覆盖有限。

P2 再实现；本期不把多数据源作为发布门槛。

### 3.7 隐私与合规

- 只向刮削服务发送游戏名称、大小、哈希、平台 ID 和用户自己的 API 凭据。
- **不发送** NAS 地址、共享路径、SMB 账号或密码。
- 刮削默认关闭，首次使用时由用户明确开启并同意服务条款。
- 无遥测、无统计上报；缓存可一键清除。
- 不内置 ROM、封面包或受版权保护的内容；用户对自己库中的内容负责。

## 4. 同类产品功能分析与取舍

| 产品 | 主要能力 | 借鉴 | 不采用 |
| --- | --- | --- | --- |
| Steam / Epic | 商店、搜索、详情、下载队列、更新、库 | 内容组织、下载队列、详情页、库管理 | 账号、支付、推荐、社交 |
| PortMaster | 目录、安装/卸载、更新、运行库 | 安装/更新/卸载流程 | 公共目录、在线仓库 |
| Grout（RomM） | 服务端库、元数据、下载、存档 | 元数据展示、库同步 | 依赖 RomM 服务端 |
| ES-DE / Skyscraper | gamelist.xml、封面/截图/简介刮削 | 元数据字段、刮削源与语言优先级 | 重型前端 |
| NAS 文件管理器 | 协议配置、目录浏览、传输队列 | 连接配置、浏览、传输体验 | 无游戏平台概念、无原厂集成 |

**保留**：连接管理、目录/清单、详情、下载/安装、队列/续传、库管理、在线刮削、原厂封面集成、离线缓存。  
**裁剪**：支付、账号注册、评论、社交、推荐算法。  
**后置**：多数据源、存档同步、应用内直接启动游戏。

## 5. 功能需求清单

**优先级：** P0 = MVP 必须；P1 = 首个正式版前后；P2 = 后续增强。

### 5.1 连接管理（CONN）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| CONN-01 | P0 | 新建/编辑/删除 SMB 连接配置：显示名、主机、端口、共享名、根目录、用户名、密码、域 | 保存后重启可读取；必填项校验 |
| CONN-02 | P0 | SMB2/3 连接，不支持 SMB1 | 可连接 Synology/QNAP/TrueNAS/Windows/Samba 并列出目录 |
| CONN-03 | P0 | 认证：guest/匿名、用户名密码、域账号 | 三种方式在真实 NAS 上验证 |
| CONN-04 | P0 | 测试连接 | 成功 5 秒内返回；失败给出中文原因（地址、认证、共享、权限、协议） |
| CONN-05 | P0 | 共享与根目录选择 | 可输入共享名并逐级浏览选择根目录；支持返回上级 |
| CONN-06 | P0 | 超时、重试、断线检测 | 拔网/关 NAS 后 30 秒内显示离线；恢复后可重连 |
| CONN-07 | P0 | 密码可保存/可不保存，输入遮罩 | 不保存时重启要求重新输入；日志不出现密码 |
| CONN-08 | P1 | 多套配置、默认配置、启动自动连接 | 可保存多套并快速切换 |
| CONN-09 | P1 | 高级 SMB 参数：端口、SMB 方言、签名/加密、DFS | 在目标 NAS 上验证；默认自动协商 |
| CONN-10 | P1 | 路径书签与最近访问 | 一键回到常用目录 |
| CONN-11 | P2 | SMB 共享枚举、mDNS 发现 | 按需实现 |
| CONN-12 | P2 | SMB1（不推荐） | 仅在明确需要时实现 |
| CONN-13 | 不做 | NFS、FTP/FTPS、SFTP | 本期不支持；保留 VFS 接口以便后续扩展 |

**SMB 注意事项：**

- 多数现代 NAS 默认启用 SMB2/3；老 NAS 需要手动开启。
- SMB 共享名和路径可能包含中文；统一 UTF-8 处理。
- 建议为掌机创建只读专用账号，仅授权游戏目录。
- 不修改 NAS 文件；删除源文件功能不提供。

### 5.2 目录浏览与游戏清单（BROWSE）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| BROWSE-01 | P0 | 扫描 `Emus/*/config.json`，展示可用平台 | 能读取 label/rompath/imgpath/extlist；缺失时使用兜底映射 |
| BROWSE-02 | P0 | SMB 目录浏览：进入、返回、刷新、面包屑 | 多级目录可用；返回保留位置 |
| BROWSE-03 | P0 | 游戏识别 | 按 `extlist` + 内置映射识别；普通文件不误判 |
| BROWSE-04 | P0 | 游戏清单：名称、平台、大小、格式、修改时间、安装状态 | 500 项 1.5 秒内加载；信息完整 |
| BROWSE-05 | P0 | 列表/网格双视图 | 1024×768 无溢出；手柄切换 |
| BROWSE-06 | P0 | 大目录虚拟滚动/分页 | 1000+ 项滚动流畅 |
| BROWSE-07 | P0 | 当前目录过滤/搜索 | 输入即时过滤；大小写不敏感 |
| BROWSE-08 | P0 | 游戏详情页 | 封面占位、标题、文件名、路径、大小、文件列表、平台；返回不丢位置 |
| BROWSE-09 | P0 | 多文件游戏分组 | `.cue`+`.bin`、`.m3u`、多碟、压缩包内多文件识别为一个条目 |
| BROWSE-10 | P1 | 全局搜索、排序、筛选 | 基于缓存；1 万条内搜索 ≤500ms；按名称/大小/时间/平台排序 |
| BROWSE-11 | P1 | 封面与元数据显示 | 优先在线刮削结果，其次本地 `.media`/`gamelist.xml`/同名图片，最后占位图 |
| BROWSE-12 | P1 | 本地元数据兼容 | 支持 `store.json`、`gamelist.xml`、同名 `.png/.jpg` |
| BROWSE-13 | P1 | 收藏、最近浏览、最近下载 | 重启后保留 |
| BROWSE-14 | P1 | 离线缓存浏览 | 断网后可看缓存清单、元数据和封面 |
| BROWSE-15 | P0 | 隐藏系统文件 | 默认隐藏 `@eaDir`、`#recycle`、`System Volume Information`、`.DS_Store`、`Thumbs.db` 等 |
| BROWSE-16 | P2 | 中文拼音排序/首字母搜索 | 可选实现 |

### 5.3 下载与安装（DL）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| DL-01 | P0 | 单游戏下载，显示进度、速度、已下载/总量、ETA | 进度误差 <2%；UI 不卡顿 |
| DL-02 | P0 | 暂停/继续/取消/重试 | 1 秒内响应；取消后清理或保留 `.part` 可选 |
| DL-03 | P0 | 断点续传 | SMB 按偏移读取；断网/杀进程/休眠后可继续；10 次中断后哈希正确 |
| DL-04 | P0 | `.part` 临时文件 + 完成后原子重命名 | 异常退出不产生被原厂菜单误识别的半成品 |
| DL-05 | P0 | 空间检查与磁盘满处理 | 下载前检查；不足时阻止并提示所需空间 |
| DL-06 | P0 | 目标目录来自 `Emus` 配置 | 写入对应 `rompath`；可手动覆盖；目标不存在时提示创建 |
| DL-07 | P0 | 冲突处理 | 覆盖/跳过/重命名/仅更新；默认询问 |
| DL-08 | P0 | 多文件/多碟整组下载 | `.cue`/`.bin`/`.m3u` 全部下载，相对路径保持 |
| DL-09 | P0 | 安装后刷新提示 | 提示 `Menu → Refresh Roms`；可选“删除缓存并提示刷新”（需确认） |
| DL-10 | P0 | 网络中断自动暂停/重试 | 断网任务保留；恢复后可继续 |
| DL-11 | P0 | 并发默认 1，最大 2 | 并发时 UI 不卡顿、Wi-Fi 不拥塞 |
| DL-12 | P1 | 队列、批量选择、全部暂停、失败重试 | 重启后队列恢复；失败项可单独重试 |
| DL-13 | P1 | 校验（大小/MD5/SHA1/SHA256） | 有哈希时校验；失败标记并可重下 |
| DL-14 | P1 | 解压/保留压缩包策略 | zip 优先；7z/RAR 后续；解压失败回滚 |
| DL-15 | P1 | 封面写入原厂 `Imgs` | 安装/刮削后写入 `Imgs/<SYSTEM>/<romname>.png`；刷新后可见 |
| DL-16 | P1 | 传输历史、失败列表、限速 | 历史可查；限速生效（可选） |
| DL-17 | P2 | 任务导入/导出、后台下载 | 原厂前台应用无真正后台，暂不做 |

**安装策略默认值：**

- 单文件 ROM：原样复制到 `rompath`。
- ZIP：优先保留；若原厂模拟器不支持该压缩格式，则按平台策略解压。
- `.cue`+`.bin`：整组复制到同一目录；多碟用 `.m3u` 组织。
- 解压/多文件安装后，必须验证原厂游戏菜单能扫描到；必要时平铺，不使用原厂不支持的子目录层级。

### 5.4 已安装与存储管理（LIB）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| LIB-01 | P1 | 扫描 `Roms` 并统计各平台数量/占用 | 与文件管理器一致；不阻塞 UI |
| LIB-02 | P1 | 已安装列表、平台筛选、大小 | 可按平台筛选；显示总占用 |
| LIB-03 | P1 | 更新检测（大小/时间/哈希） | 标记可更新；不误报 |
| LIB-04 | P1 | 删除游戏（二次确认） | 只删允许目录；不删存档/BIOS/系统文件 |
| LIB-05 | P1 | 空间总览与清理 | 显示 SD 总/剩余、缓存、临时文件；可清理 |
| LIB-06 | P1 | 孤儿 `.part` 清理 | 可识别并安全删除 |
| LIB-07 | P2 | 重复游戏检测 | 按哈希/大小+名称提示 |
| LIB-08 | P2 | 存档同步 | 需单独设计，不在本期 |
| LIB-09 | P2 | 应用内直接启动游戏 | 依赖原厂启动机制，M0 评估 |

### 5.5 在线刮削与元数据（META）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| META-01 | P0 | Provider 框架 + ScreenScraper 接入 | 可在设置中填入 `devid/devpassword/ssid/sspassword/softname`；配置校验 |
| META-02 | P0 | 单游戏手动刮削 | 详情页可触发；显示进度/结果/错误；不阻塞 UI |
| META-03 | P0 | 元数据字段 | 标题、简介、类型、开发商、发行商、发售日、人数、评分、封面、截图 |
| META-04 | P0 | 语言与回退 | 优先 `zh`，缺失回退 `en`；界面可切换 |
| META-05 | P0 | 匹配策略 | 哈希优先；失败用名称+平台搜索；可手动选择匹配项 |
| META-06 | P0 | 配额与限速 | 串行请求；解析配额；超限停止并提示；不无限重试 |
| META-07 | P0 | 缓存 | 元数据/媒体缓存到本地；断网可看；TTL 可配置 |
| META-08 | P0 | 封面写入原厂 `Imgs` | PNG、文件名匹配 ROM、默认约 300px 宽；写入后可提示刷新 |
| META-09 | P0 | 合规与隐私 | 首次开启需同意条款；显示数据来源；不发送 NAS 凭据；无遥测 |
| META-10 | P1 | 批量刮削 | 按平台/全部游戏排队；进度、暂停、继续、失败重试；可断点续跑 |
| META-11 | P1 | 本地元数据兜底/合并 | `store.json`、`gamelist.xml`、同名图片；在线失败不覆盖本地 |
| META-12 | P1 | 重新刮削/刷新 | 手动强制刷新；清除缓存 |
| META-13 | P2 | 手动编辑元数据 | 可改标题/简介/封面；本地保存 |
| META-14 | P2 | 多数据源 | TheGamesDB/IGDB/GameTDB/Bangumi（libretro-thumbnails 已实现） |
| META-15 | P2 | 中文增强 | 中文标题/简介补充源或本地翻译表 |

**ScreenScraper 凭据说明：**

- `devid/devpassword` 是开发者凭据，需要用户自行向 ScreenScraper 申请，不能由应用内置。
- `ssid/sspassword` 是普通用户账号，建议填写以提高配额。
- 若用户没有任何凭据，在线刮削不可用；应用仍可用文件名/本地图片展示。
- 上线前需核对 ScreenScraper 的 API 使用条款，确认开源应用的使用方式与署名要求。

### 5.6 设置与本地化（SET）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| SET-01 | P0 | 首次启动向导 | 语言 → SMB 连接 → 根目录 → 测试 → 完成；可跳过 |
| SET-02 | P0 | 简中/英文切换 | 即时生效；重启保持 |
| SET-03 | P0 | 网络超时、并发、临时目录、缓存上限 | 默认值可用；高级项可改 |
| SET-04 | P0 | 刮削设置：Provider、凭据、语言、地区、媒体类型、自动刮削 | 配置校验；可清除凭据 |
| SET-05 | P0 | 日志查看/导出/清理 | 可导出到 SD；日志脱敏 |
| SET-06 | P0 | 关于页：版本、开源许可、隐私说明、数据来源署名 | 信息完整 |
| SET-07 | P1 | 主题、字号、列表/网格默认视图 | 深色/浅色/高对比；字号 3 档 |
| SET-08 | P1 | 手柄提示、按键重复速度 | 提示随上下文变化 |
| SET-09 | P1 | 配置备份/恢复/重置 | 导出 JSON；导入校验 |
| SET-10 | P2 | 应用自更新 | 检查 GitHub Release 或 NAS 清单 |
| SET-11 | P2 | 时间/电量/截图 | 不影响主流程 |

### 5.7 系统集成与交互（SYS）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| SYS-01 | P0 | 原厂 `Apps/Nascade.pak` 打包、安装、启动、退出 | 拷入 `Apps` 后出现在 Apps 菜单；退出返回菜单 |
| SYS-02 | P0 | 全手柄导航 | 所有界面仅用手柄完成；无死焦点 |
| SYS-03 | P0 | 1024×768 适配 | 无拉伸/溢出；安全边距 ≥24px |
| SYS-04 | P0 | 中文字体与日文假名 | 游戏名/简介/设置无方框；字体缓存正常 |
| SYS-05 | P0 | 屏幕键盘 | 可输入 IP、共享名、账号、密码、路径；密码遮罩 |
| SYS-06 | P0 | 休眠/唤醒/SIGTERM | 保存队列与进度；唤醒可续传；异常退出不损坏文件 |
| SYS-07 | P0 | 日志写入 `.Nascade/logs/` | 滚动；脱敏；可导出 |
| SYS-08 | P0 | 原厂按键布局适配 | A/B 确认返回符合直觉；可在设置切换 |
| SYS-09 | P0 | 刷新原厂游戏库 | 安装后提示 `Menu → Refresh Roms`；可选删除缓存前确认 |
| SYS-10 | P1 | 应用图标/选中态适配 | 与原厂 Apps 菜单风格一致；真机验证 |
| SYS-11 | P2 | 原厂游戏菜单直接启动本应用内选中的游戏 | 依赖原厂机制，M0 评估 |
| SYS-12 | P2 | 无障碍/大字号/高对比 | 可读可操作 |

**交互约定：**

- 方向键/左摇杆：移动焦点；A：确认/进入；B：返回/取消；X：次要操作（下载/暂停/刮削）；Y：视图切换；L/R：翻页；Start：菜单/退出。
- 文本输入用屏幕键盘；密码默认遮罩。
- 下载或刮削进行中退出：提示任务会暂停，下次继续。
- 退出应用后回原厂 Apps 菜单，不重启设备。

### 5.8 可靠性与安全（REL/SEC）

| ID | 优先级 | 需求 | 验收标准 |
| --- | --- | --- | --- |
| REL-01 | P0 | 网络 I/O 不阻塞 UI | 列表/下载/刮削时 UI 响应 |
| REL-02 | P0 | 统一错误模型 | 覆盖主机不可达、认证失败、权限不足、共享不存在、磁盘满、配额超限、TLS 失败 |
| REL-03 | P0 | 队列/配置持久化 | 强杀后重启可恢复；事务提交 |
| REL-04 | P0 | 断线重连与退避 | 不无限重试；可取消 |
| REL-05 | P0 | 路径安全 | 拒绝 `..`、绝对路径注入、符号链接逃逸；删除限定根目录 |
| REL-06 | P0 | 源端只读 | 浏览/下载不修改 NAS 文件 |
| REL-07 | P0 | SMB 凭据保护 | 仅本地保存；可选不保存；日志脱敏 |
| REL-08 | P0 | 刮削凭据与隐私 | 凭据本地保存；可清除；不上传 NAS 信息；无遥测 |
| REL-09 | P0 | TLS 校验 | HTTPS 默认验证证书；内置 CA 包；失败给出可操作提示 |
| REL-10 | P0 | 合规提示 | 不提供游戏内容；用户自担合规责任；展示刮削数据来源 |
| REL-11 | P1 | 配置/数据库损坏恢复 | 自动备份；损坏可重建 |
| REL-12 | P1 | 下载/刮削校验 | 哈希/大小校验；失败可重下/重刮 |
| REL-13 | P1 | 弱网测试 | 高延迟/丢包/低带宽不崩溃 |
| REL-14 | P2 | 本地崩溃日志导出 | 仅用户主动导出 |

**安全建议（用户手册）：**

- NAS 使用只读专用账号，仅授权游戏目录。
- 仅局域网使用；公网访问请走 VPN，不直接暴露 SMB。
- SD 卡（FAT32/exFAT）无法提供可靠的文件权限保护；保存密码有风险。
- 刮削服务的账号/密钥同样保存在 SD 卡，需自行评估。

## 6. 非功能需求（NFR）

| ID | 维度 | 指标/要求 |
| --- | --- | --- |
| NFR-01 | 性能 | 冷启动 ≤3 秒（5k 缓存、无网络）；页面切换 ≤100ms；500 项目录 ≤1.5 秒；缓存命中 ≤200ms |
| NFR-02 | 资源 | 常驻 ≤150MB、峰值 ≤250MB；下载时单核 CPU ≤50%；包体 ≤50MB（含字体/CA） |
| NFR-03 | 传输 | 吞吐 ≥ 实测 Wi-Fi 80%；断点续传 10 次中断哈希正确；24 小时队列稳定 |
| NFR-04 | SMB 兼容 | Synology DSM、QNAP QTS、TrueNAS、Windows 11、Samba；SMB2/3；签名/加密自动协商 |
| NFR-05 | 刮削 | 单游戏响应 ≤10 秒（网络正常）；批量 100 个游戏可暂停/续跑；严格遵守配额与限速 |
| NFR-06 | 文件 | FAT32/exFAT；中/日文名；>4GB 提前警告；1 万条索引；单目录 1000 项 |
| NFR-07 | 网络 | 2.4GHz 802.11n、WPA2；弱信号可重连 |
| NFR-08 | 可靠性 | 异常下载/安装不破坏已有文件；删除二次确认；掉电可恢复 |
| NFR-09 | 可维护 | 模块化 VFS/Provider；单元测试；桌面调试构建；CI 产出 pak |
| NFR-10 | 合规 | 开源许可；libsmb2/libcurl 等第三方许可；不内置 ROM；遵守刮削服务条款 |
| NFR-11 | 可测试 | Docker 模拟 SMB 服务 + ScreenScraper 录制/模拟响应 + 真机 smoke test |

## 7. 技术方案建议

### 7.1 总体架构

```text
+--------------------------------------------------------------+
| UI 层（SDL2 + SDL2_ttf + stb_image）                          |
| 首页 / 平台 / 清单 / 详情 / 下载队列 / 刮削 / 设置 / 向导     |
+--------------------------------------------------------------+
| 应用核心                                                       |
| 状态机 / 路由 / 焦点 / i18n / 配置 / 任务调度 / 错误映射       |
+------------------+------------------+------------------------+
| VFS 抽象          | 传输引擎          | 元数据 Provider         |
| SMB2/3 libsmb2   | 队列/续传/校验    | ScreenScraper (libcurl) |
| 本地文件后端      | zip 解压/miniz    | 本地 store.json/xml     |
+------------------+------------------+------------------------+
| SQLite：配置 / 索引 / 下载任务 / 刮削缓存 / 媒体索引           |
+--------------------------------------------------------------+
| SD 卡：Roms / Imgs / Emus / Apps / .Nascade                |
+--------------------------------------------------------------+
```

### 7.2 技术选择

| 领域 | 方案 | 理由 |
| --- | --- | --- |
| 语言 | C++17 | 与 SDL2/嵌入式库契合；GCC 7.5 可用 |
| UI | SDL2 + SDL2_ttf + stb_image | 原厂已有 SDL2/SDL2_ttf；stb_image 避免 SDL2_image 依赖 |
| SMB | libsmb2 | 用户态 SMB2/3，无需 root/Samba |
| HTTPS/刮削 | libcurl + 内置 CA | 成熟 TLS/HTTP；支持 JSON API 和媒体下载 |
| 元数据解析 | nlohmann/json | 单头文件，开发效率高 |
| 数据库 | SQLite3 amalgamation | 单文件、事务、缓存/队列 |
| ZIP | miniz | 轻量；先支持 zip，7z/RAR 后续 |
| 构建 | CMake + 官方 SDK | 与 Stock OS ABI 匹配；可复现 |
| 测试 | 轻量单元测试 + Docker SMB | 不引入重型测试框架 |
| 打包 | 原厂 `Apps/Nascade.pak` | 与 Stock OS 集成 |

### 7.3 为什么用用户态 SMB 库

- 应用无 root，不能依赖 `mount.cifs`。
- 用户态库可按连接配置超时、取消、重试；错误可控。
- 不修改系统分区，不需要安装 Samba 客户端。
- 同一 VFS 接口后续可扩展 NFS/FTP，但本期只实现 SMB。

### 7.4 线程模型

- 主线程：SDL 事件、渲染、焦点；SDL 纹理只在主线程创建。
- I/O 线程：SMB 连接/列目录/下载；可取消。
- 刮削线程：HTTP 请求/解析/媒体下载；串行队列，遵守限速。
- 封面线程：解码/缩放；像素数据交主线程上传纹理。
- UI 更新通过线程安全队列/原子变量；进度刷新 4–10Hz。
- SQLite 单进程串行访问；SD 卡上使用 `journal_mode=DELETE`、`synchronous=FULL`，避免 FAT32 WAL 风险。

### 7.5 构建与打包

- 目标：官方 `trimui/toolchain_sdk_smartpro` 20231018（Linaro GCC 7.5.0 + `SDK_usr_tg5040_a133p` + SDL2 2.26.1.GE8300）。
- 在 Linux/Docker/CI 中构建；Brick Pro 的 Stock OS 兼容性用真机 smoke test 验证。
- 应用包：`Apps/Nascade.pak/`，含 `config.json`、`launch.sh`、`icon.png`、二进制、必要 `.so`、字体、CA。
- 应用数据：`/mnt/SDCARD/.Nascade/`（配置、缓存、日志），避免随 pak 升级丢失。
- 桌面调试：Windows 上可用便携 MSYS2 构建 SDL2 桌面版；SMB 用本地 mock/测试服务器；目标构建走 CI。

### 7.6 M0 技术验证（Go/No-Go）

1. 原厂 Apps 包：`Apps/Nascade.pak` 能出现在 Apps 菜单，SDL2 窗口 1024×768、中文字体、手柄、退出正常。
2. 解析 `Emus/*/config.json`，正确得到平台、`rompath`、`imgpath`、`extlist`。
3. SMB2/3 连接真实 NAS，列出 500 项目录，下载 100MB。
4. 杀进程/休眠/断网后续传，最终哈希正确。
5. 写入测试 ROM 到 `Roms`、封面到 `Imgs`，执行 Refresh Roms 后原厂菜单可见。
6. ScreenScraper：用真实凭据调用 `jeuInfos.php`，验证 `langue=zh`、媒体下载、配额/限速。
7. 性能：启动、列表、下载吞吐、CPU、内存。
8. 官方 SDK 构建产物在 Brick Pro Stock OS 上运行；确认 SDL2/glibc 兼容。

> M0 任一项失败，先调整技术方案或范围，再进入 M1。

## 8. SD 卡目录与平台映射

### 8.1 实际目录（推荐）

```text
/mnt/SDCARD/
├── Apps/
│   └── Nascade.pak/
│       ├── config.json
│       ├── icon.png
│       ├── launch.sh
│       └── nascade
├── Emus/
│   ├── FC/config.json
│   ├── SFC/config.json
│   └── GBA/config.json
├── Roms/
│   ├── FC/
│   ├── SFC/
│   ├── GBA/
│   └── PS/
├── Imgs/
│   ├── FC/
│   ├── SFC/
│   └── GBA/
├── RetroArch/
└── .Nascade/
    ├── config.json
    ├── cache/
    └── logs/
```

### 8.2 平台发现算法

1. 扫描 `/mnt/SDCARD/Emus/*/config.json`。
2. 解析 `label`、`rompath`、`imgpath`、`extlist`。
3. 把 `rompath`/`imgpath` 解析为绝对路径（相对 `Emus/<dir>/`）。
4. 校验目录是否存在；不存在时提示或跳过。
5. 与内置短标签表合并；同一短标签以 `Emus` 配置为准。
6. 在 UI 中展示平台列表；用户可在设置中覆盖目标目录。

### 8.3 内置兜底映射（示例）

| 短标签 | 平台 | 常见扩展名 |
| --- | --- | --- |
| `FC` | NES / Famicom | `.nes` `.fds` `.zip` |
| `SFC` | SNES / Super Famicom | `.sfc` `.smc` `.zip` |
| `GB` | Game Boy | `.gb` `.zip` |
| `GBC` | Game Boy Color | `.gbc` `.zip` |
| `GBA` | Game Boy Advance | `.gba` `.zip` |
| `MD` | Sega Genesis / Mega Drive | `.md` `.gen` `.bin` `.zip` |
| `PS` | PlayStation | `.cue`+`.bin` `.chd` `.pbp` `.m3u` |
| `N64` | Nintendo 64 | `.z64` `.n64` `.v64` `.zip` |
| `PCE` | PC Engine | `.pce` `.zip` |
| `ARCADE` | Arcade | `.zip` `.7z` |
| `PSP` | PlayStation Portable | `.iso` `.cso` `.pbp` |
| `DC` | Dreamcast | `.gdi` `.cdi` `.chd` |

> 实际以 `Emus` 配置为准；表仅用于首次启动或配置缺失时的兜底。

### 8.4 封面命名

- 目标：`Imgs/<SYSTEM>/<romname>.png`。
- `<romname>` 是 ROM 主文件名去掉扩展名；多文件游戏用 `.cue`/`.m3u` 的主名。
- 统一转 PNG；默认宽度约 300px；保留比例。
- 文件名中的非法字符按原厂规则处理；覆盖前二次确认。
- 写入后提示刷新/重启原厂游戏菜单。

### 8.5 `store.json`（可选）

如果 NAS 根目录存在 `store.json`，可用于补充标题、简介、封面、版本、哈希；没有时自动扫描目录。格式与 v0.1 相同，字段包括：

- `schema_version`
- `store_name`
- `root`
- `platforms[]`：`id`、`name`、`tag`、`path`
- `games[]`：`id`、`title`、`platform`、`files[]`、`description`、`cover`、`screenshots`、`developer`、`release_date`、`tags`、`install`

`install.mode` 支持 `copy` / `extract`；`install.dest` 可覆盖原厂 `rompath`。

### 8.6 路径与编码

- 统一 UTF-8；SMB 返回的路径按 UTF-8 处理。
- 过滤 FAT32/exFAT 非法字符；冲突提示。
- 大小写不敏感文件系统需检测同名冲突。
- 不跟随符号链接；如支持，必须限制在配置根目录内。
- 长路径/深层目录按原厂扫描能力限制；M0 验证子目录是否被扫描。

## 9. 里程碑与验收

| 阶段 | 目标 | 交付物 | 粗略时间 |
| --- | --- | --- | --- |
| M0 技术验证 | 证明原厂 SDL2/Apps、SMB、续传、刮削可行 | 最小 pak、协议/刮削 demo、M0 报告、性能数据 | 约 5–10 个工作日 |
| M1 MVP | P0 需求 | 可安装的 `Apps/Nascade.pak`、连接/浏览/详情/刮削/下载/安装、中文 UI、日志 | 约 2–3 周 |
| M2 增强 | P1 需求 | 队列/批量、批量刮削、库管理、缓存、离线、图标优化 | 约 2–3 周 |
| M3 发布 | 发布与打磨 | 文档、CI、Release 包、真机测试记录 | 按需 |

> 时间为粗略估算，前提是开发环境、真机和 NAS 可用；不作为固定承诺。

### 9.1 MVP 验收清单

- 在真实 Brick Pro（Stock OS）+ 真实 NAS 上完成 SMB 浏览、详情、刮削、下载、安装。
- 100MB 文件中断 3 次后可续传，最终哈希/大小正确。
- 中文界面与中文/日文游戏名显示正常。
- 刮削封面/简介可显示，封面可写入 `Imgs` 并在原厂菜单刷新后显示。
- 安装后按提示 Refresh Roms，原厂游戏菜单能看到新游戏。
- 日志可导出且不含密码/API 密钥。
- pak 拷入 `Apps` 即可安装，无 root 依赖。

## 10. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| Brick Pro Stock OS 与 Brick TG3040 基础包不同 | 打包/路径不兼容 | M0 真机确认；以 `Emus`/`Apps` 实际配置为准；保留兜底 |
| 原厂 SDL2 版本/依赖不匹配 | 应用无法启动 | 先做 hello-world pak；必要时随包带 `.so` 和 `LD_LIBRARY_PATH` |
| ScreenScraper 开发者凭据难获取 | 在线刮削不可用 | 设置中支持用户凭据；提供本地元数据兜底；文档说明申请流程 |
| ScreenScraper 配额/限流 | 批量刮削中断 | 串行、限速、缓存、配额解析、可续跑 |
| 中文元数据覆盖不足 | 标题/简介仍为英文 | `zh`→`en` 回退；后续接 Bangumi 等中文源 |
| SMB 兼容性差异 | 连接/列目录失败 | libsmb2 + 真机矩阵；可调方言/签名；错误提示 |
| Wi-Fi 慢/不稳 | 下载失败 | 单任务、续传、重试；明确前台运行限制 |
| FAT32 4GB | 大文件失败 | 下载前检测文件系统/大小并警告；建议 exFAT |
| 原厂菜单不刷新 | 安装后看不到游戏 | 提示 Refresh Roms；M0 验证缓存删除方式 |
| 应用无真正后台 | 退出/休眠中断 | 保存状态；唤醒续传；用户提示 |
| 构建工具链缺失 | 无法出包 | 官方 SDK + CI；桌面端 MSYS2；M0 先搭好 |
| 范围蔓延 | 延期 | P0 门禁；NFS/FTP/多固件全部后置 |

## 11. 已确认决策与待确认事项

### 11.1 已确认

1. 目标固件：原厂 TrimUI OS。
2. 协议：仅 SMB2/3。
3. 功能：浏览 + 下载安装。
4. 内容来源：扫描 NAS 目录 + 可选 `store.json`。
5. 技术栈：C++17 + SDL2。
6. 元数据：默认 libretro 封面刮削，ScreenScraper 文字元数据可选。
7. 安装：默认原样复制，按平台可选解压。
8. 语言：简中默认 + 英文。
9. 发布：GitHub 开源；原厂 `Apps` 手动安装。
10. 名称：Nascade（NAS + Arcade + cascade）。

### 11.2 需要你提供/确认

1. **ScreenScraper 账号**：是否已有 `ssid/sspassword`？是否愿意申请 `devid/devpassword`？如果没有，在线刮削需要先解决凭据来源。
2. **NAS 信息**：品牌/系统、SMB 共享路径、是否有中文目录、测试用游戏目录结构。
3. **Brick Pro Stock OS 版本**：系统版本号；SD 卡上是否有 `Emus`、`Apps`、`Roms`、`Imgs`，能否提供 `Emus` 某个 `config.json` 样例（可脱敏）。
4. **中文元数据优先级**：按推荐 `zh` 优先、缺失回退 `en`，是否接受？
5. **批量刮削**：本期先做单游戏手动刮削，批量放 P1；是否接受？
6. **构建环境**：本机 Windows 没有 g++/CMake/Docker。计划：桌面调试用便携 MSYS2；目标构建用官方 SDK + CI。是否有更合适的 Linux/Docker 环境？
7. **应用图标/名称**：名称已确定为 Nascade；图标当前使用占位图，后续可替换。

## 12. 参考资料

- TrimUI Brick Pro 规格与发布信息：<https://www.notebookcheck-cn.com/TrimUI-Brick-Pro-Game-Boy.1340153.0.html>
- TrimUI 官方产品页：<https://trimui.net/products/trimui-brick-pro-with-64gb-sd-card>
- 原厂 SD 基础包：<https://github.com/trimui/assets_brick>
- 官方工具链/SDK：<https://github.com/trimui/toolchain_sdk_smartpro>
- TrimUI Smart Pro 社区工具链（Docker）：<https://github.com/s0ckz/trimui-smart-pro-toolchain>
- Stock OS 应用示例（LedControl）：<https://github.com/ro8inmorgan/LedControl>
- Brick SDL2 应用参考（SDLReader）：<https://github.com/Helaas/SDLReader-brick>
- TrimUI Brick 原厂 ROM 目录/封面说明：<https://retrohandhelds.online/trimui-brick-setup-guide/>
- RomM TrimUI 平台映射：<https://grout.romm.app/platforms/trimui/>
- ScreenScraper API：<https://www.screenscraper.fr/webapi2.php>
- Skyscraper 语言支持（含 zh）：<https://gemba.github.io/skyscraper/LANGUAGES/>
- ES-DE ScreenScraper 多语言说明：<https://gitlab.com/es-de/emulationstation-de/-/raw/master/USERGUIDE-DEV.md>
- TrimUI Brick 手动刷新 ROM/缓存说明：<https://zh-cn.manuals.plus/ru/trimui-brick/tg3040-handheld-game-console-manual>
