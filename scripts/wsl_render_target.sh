#!/bin/sh
# 用 qemu 的 SDL offscreen 驱动渲染目标版首页截图。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
STAGE=/tmp/nascade_pkg

rm -rf "$STAGE"
mkdir -p "$STAGE"
cd "$STAGE"
NASCADE_ZIP="$ROOT/dist/Nascade.pak.zip" python3 -c 'import os, zipfile; zipfile.ZipFile(os.environ["NASCADE_ZIP"]).extractall(".")'
cd "$STAGE/Nascade.pak"
chmod +x nascade launch.sh
export LD_LIBRARY_PATH="$PWD/lib:$LIBC/lib:$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
export NASCADE_SD_ROOT="$ROOT/devdata"
rm -f /tmp/nascade_target_home.ppm
qemu-aarch64-static -L "$LIBC" ./nascade --screenshot /tmp/nascade_target_home.ppm --screen home
ls -lh /tmp/nascade_target_home.ppm
mkdir -p "$ROOT/screenshots"
python3 "$ROOT/tools/ppm_to_png.py" /tmp/nascade_target_home.ppm "$ROOT/screenshots/target_home.png"
