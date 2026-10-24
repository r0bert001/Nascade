#!/bin/sh
# 解包后补齐执行权限，并在 qemu 下执行下载测试。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
STAGE=/tmp/nascade_pkg

rm -rf "$STAGE"
mkdir -p "$STAGE"
cd "$STAGE"
NASCADE_ZIP="$ROOT/build/Nascade.pak.zip" python3 -c 'import os, zipfile; zipfile.ZipFile(os.environ["NASCADE_ZIP"]).extractall(".")'
cd "$STAGE/Nascade.pak"
ls -la
chmod +x nascade launch.sh
export LD_LIBRARY_PATH="$PWD/lib:$LIBC/lib:$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
qemu-aarch64-static -L "$LIBC" ./nascade --help | head -n 3

export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_LOCAL_NAS="$ROOT/devdata/NasRoot"
export NASCADE_REMOTE_PATH='GBA/NAS Test Game.gba'
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test Game Packaged.gba"
export NASCADE_REMOTE_SIZE=131072
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
qemu-aarch64-static -L "$LIBC" ./nascade --test-download
echo DL_EXIT=$?
sha1sum "$ROOT/devdata/NasRoot/GBA/NAS Test Game.gba" "$NASCADE_LOCAL_PATH"
