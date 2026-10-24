#!/usr/bin/env python3
"""生成桌面调试用的模拟 SD 卡数据。

所有 ROM 都是零填充或文本占位文件，不包含真实游戏内容。
这些文件只应保留在本地，`devdata/` 已加入 .gitignore。
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

PLATFORMS = {
    "FC": {"extlist": "nes|fds|zip", "demo": "Demo FC Game.nes"},
    "GBA": {"extlist": "gba|zip", "demo": "Demo GBA Game.gba"},
    "MD": {"extlist": "md|gen|bin|zip", "demo": "Demo MD Game.md"},
    "PS": {"extlist": "cue|bin|chd|pbp|m3u", "demo": "Demo PS Game.cue"},
    "SFC": {"extlist": "sfc|smc|zip", "demo": "Demo SFC Game.sfc"},
}


def write_placeholder(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("demo rom placeholder\n", encoding="utf-8")


def write_zero_file(path: Path, size: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\0" * size)


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "devdata")
    out.mkdir(parents=True, exist_ok=True)

    for platform, info in PLATFORMS.items():
        config = {
            "label": platform,
            "rompath": f"../../Roms/{platform}",
            "imgpath": f"../../Imgs/{platform}",
            "extlist": info["extlist"],
            "launch": "launch.sh",
        }
        config_path = out / "Emus" / platform / "config.json"
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text(json.dumps(config, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        write_placeholder(out / "Roms" / platform / info["demo"])

    # 模拟 NAS 上的下载源文件；内容为零，仅用于验证下载和校验流程。
    write_zero_file(out / "NasRoot" / "GBA" / "NAS Test Game.gba", 128 * 1024)
    write_zero_file(out / "NasRoot" / "FC" / "NAS Test NES.nes", 64 * 1024)

    print(f"devdata written: {out.resolve()}")
    print("Generated synthetic Emus configs, placeholder ROMs and zero-filled test files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
