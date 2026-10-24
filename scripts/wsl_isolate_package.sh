#!/bin/sh
# 分别用系统库和包内库做 qemu 下载冒烟测试。
set -u

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
LIBSTD="$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
SYS="$CROSS_DIR/sysroot/usr/lib"
STAGE=/tmp/nascade_pkg

export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_LOCAL_NAS="$ROOT/devdata/NasRoot"
export NASCADE_REMOTE_PATH='GBA/NAS Test Game.gba'
export NASCADE_REMOTE_SIZE=131072

echo '=== stripped binary + original libs ==='
BIN="$ROOT/build/tg5040/nascade"
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test A.gba"
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
LD_LIBRARY_PATH="$LIBC/lib:$LIBSTD:$SYS" qemu-aarch64-static -L "$LIBC" "$BIN" --test-download
echo EXIT=$?

echo '=== unstripped binary + packaged libs ==='
BIN="$CROSS_DIR/build-nascade/bin/nascade"
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test B.gba"
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
LD_LIBRARY_PATH="$STAGE/Nascade.pak/lib:$LIBC/lib:$LIBSTD" qemu-aarch64-static -L "$LIBC" "$BIN" --test-download
echo EXIT=$?

echo '=== stripped binary + packaged libs, loader list ==='
BIN="$STAGE/Nascade.pak/nascade"
LD_LIBRARY_PATH="$STAGE/Nascade.pak/lib:$LIBC/lib:$LIBSTD" qemu-aarch64-static -L "$LIBC" "$BIN" --help >/dev/null
echo HELP_EXIT=$?
LD_LIBRARY_PATH="$STAGE/Nascade.pak/lib:$LIBC/lib:$LIBSTD" qemu-aarch64-static -L "$LIBC" "$BIN" --test-download
echo DL_EXIT=$?
