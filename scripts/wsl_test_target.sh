#!/bin/sh
# 在 qemu 下对目标二进制做启动和下载测试。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
BIN="$CROSS_DIR/build-nascade/bin/nascade"
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
LIBSTD="$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
SYS="$CROSS_DIR/sysroot/usr/lib"

export DEBIAN_FRONTEND=noninteractive
apt-get install -y --no-install-recommends qemu-user-static >/tmp/qemu_install.log 2>&1 \
  || (tail -n 30 /tmp/qemu_install.log; exit 1)
export QEMU_LD_PREFIX="$LIBC"
export LD_LIBRARY_PATH="$LIBC/lib:$LIBSTD:$SYS"

echo "--- qemu help ---"
qemu-aarch64-static -L "$LIBC" -E LD_LIBRARY_PATH="$LD_LIBRARY_PATH" "$BIN" --help | head -n 25

echo "--- target download smoke test ---"
export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_LOCAL_NAS="$ROOT/devdata/NasRoot"
export NASCADE_REMOTE_PATH='GBA/NAS Test Game.gba'
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test Game Target.gba"
export NASCADE_REMOTE_SIZE=131072
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
qemu-aarch64-static -L "$LIBC" -E LD_LIBRARY_PATH="$LD_LIBRARY_PATH" "$BIN" --test-download
sha1sum "$ROOT/devdata/NasRoot/GBA/NAS Test Game.gba" "$NASCADE_LOCAL_PATH"
