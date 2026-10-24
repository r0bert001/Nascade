# 原厂固件安装包

## 目录

```text
Apps/
└── Nascade.pak/
    ├── config.json
    ├── icon.png            # 300x300，原厂 1.1.1 使用 icontop
    ├── launch.sh
    ├── nascade          # 目标 aarch64 Linux 二进制（构建产物）
    ├── lib/                # 可选的随包 .so
    └── assets/
        ├── fonts/NotoSansCJKsc-Regular.otf
        └── cacert.pem
```

## 原厂 1.1.1 的 config.json

参考 BrickTools（原厂 1.1.1）：

```json
{
  "label": "Nascade",
  "label.ch.lang": "Nascade",
  "icon": "",
  "icontop": "icon.png",
  "launch": "launch.sh",
  "description": "NAS 游戏库"
}
```

- `label.ch.lang`：中文菜单名。需要在 `/mnt/SDCARD/Apps/show.json` 中保留同名条目，否则原厂菜单可能不显示。
- `icontop`：300×300 顶层图标；`icon` 可为空。
- `launch`：启动脚本；应用退出后返回 Apps 菜单。

## 安装

1. 将 `Nascade.pak` 整个目录复制到 SD 卡根目录的 `Apps/` 下。
2. 安全弹出 SD 卡，插回 Brick Pro，重启设备。
3. 打开原厂的 Apps 菜单，运行 Nascade。

## 首次配置 SMB

首次运行后，应用会在 SD 根目录生成：

```text
/mnt/SDCARD/.Nascade/config.json
```

也可以在电脑上把包里的 `config.example.json` 复制为这个文件，填写：

- `smb.host`：NAS IP
- `smb.share`：SMB 共享名
- `smb.root`：共享内游戏根目录，例如 `/roms` 或 `/`
- `smb.username` / `smb.password`
- `smb.port`：通常是 445

然后进入应用 `SMB 连接 → 测试连接`。M0 暂未实现应用内键盘输入，这是临时配置方式。

刮削默认使用 libretro thumbnails（`scraper.provider = "libretro"`，无需凭据）。如需 ScreenScraper 的文字元数据，把 provider 改为 `screenscraper`，再填写 `scraper.devId` / `devPassword`（以及可选 `ssId` / `ssPassword`）。

应用数据、缓存和日志位于 SD 根目录的 `.Nascade/`：

```text
/mnt/SDCARD/.Nascade/
├── config.json
├── cache/
└── logs/
```

## M0 说明

- `nascade` 需要在 Linux/aarch64 环境中用官方 SDK 构建，Windows 桌面的 `.exe` 不能直接放进这个包。
- 当前配置按原厂 1.1.1 的 BrickTools 格式：`icon` 留空，使用 `icontop`（300×300），并提供 `label.ch.lang`。
- 首次测试请先确认原厂 Apps 菜单能显示图标并启动应用，再验证 SMB 与 libretro 封面刮削。
- 若原厂系统提示缺少运行库，把所需的 `.so` 放到 `lib/`；`launch.sh` 已设置 `LD_LIBRARY_PATH`。
