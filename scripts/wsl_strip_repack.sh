#!/bin/sh
# 对已收集的二进制执行 strip 并重新打包测试。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
DEST="$ROOT/build/tg5040"
STRIP="$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-strip"
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
STAGE=/tmp/nascade_pkg

chmod +x "$DEST/nascade"
"$STRIP" --strip-unneeded "$DEST/nascade"
for f in "$DEST"/lib/*.so*; do
  "$STRIP" --strip-unneeded "$f" || true
done
ls -lh "$DEST/nascade" "$DEST"/lib/*

cd "$ROOT"
sh scripts/package_stockos.sh "$DEST"
ls -lh "$ROOT/build/Nascade.pak.zip"

echo '--- qemu packaged smoke ---'
rm -rf "$STAGE"
mkdir -p "$STAGE"
cd "$STAGE"
NASCADE_ZIP="$ROOT/build/Nascade.pak.zip" python3 -c 'import os, zipfile; zipfile.ZipFile(os.environ["NASCADE_ZIP"]).extractall(".")'
cd "$STAGE/Nascade.pak"
export LD_LIBRARY_PATH="$PWD/lib:$LIBC/lib:$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
qemu-aarch64-static -L "$LIBC" ./nascade --help | head -n 3

export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_LOCAL_NAS="$ROOT/devdata/NasRoot"
export NASCADE_REMOTE_PATH='GBA/NAS Test Game.gba'
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test Game Packaged.gba"
export NASCADE_REMOTE_SIZE=131072
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
qemu-aarch64-static -L "$LIBC" ./nascade --test-download
sha1sum "$ROOT/devdata/NasRoot/GBA/NAS Test Game.gba" "$NASCADE_LOCAL_PATH"
