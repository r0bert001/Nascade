#!/bin/sh
# 构建目录 -> 原厂 Apps 包 -> 安装包冒烟测试。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-/opt/tg5040/build-nascade}
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
DEST="$ROOT/build/tg5040"
STRIP="$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-strip"
S="$CROSS_DIR/sysroot/usr/lib"
T="$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
STAGE=/tmp/nascade_pkg

rm -rf "$DEST"
mkdir -p "$DEST/lib"
cp "$BUILD_DIR/bin/nascade" "$DEST/nascade"
cp -L "$S/libSDL2-2.0.so.0.2600.1" "$DEST/lib/libSDL2-2.0.so.0"
cp -L "$S/libSDL2_ttf-2.0.so.0.10.3" "$DEST/lib/libSDL2_ttf-2.0.so.0"
cp -L "$S/libfreetype.so.6.12.1" "$DEST/lib/libfreetype.so.6"
cp -L "$S/libz.so.1.2.8" "$DEST/lib/libz.so.1"
cp -L "$S/libbz2.so.1.0.6" "$DEST/lib/libbz2.so.1.0"
cp -L "$T/libstdc++.so.6.0.24" "$DEST/lib/libstdc++.so.6"
cp -L "$T/libgcc_s.so.1" "$DEST/lib/libgcc_s.so.1"
for f in "$DEST/nascade" "$DEST"/lib/*.so*; do
  "$STRIP" --strip-debug "$f" || true
done
chmod +x "$DEST/nascade"
ls -lh "$DEST/nascade" "$DEST"/lib/*

cd "$ROOT"
sh scripts/package_stockos.sh "$DEST"
ls -lh "$ROOT/build/Nascade.pak.zip"

echo '--- packaged smoke ---'
rm -rf "$STAGE"
mkdir -p "$STAGE"
cd "$STAGE"
NASCADE_ZIP="$ROOT/build/Nascade.pak.zip" python3 -c 'import os, zipfile; zipfile.ZipFile(os.environ["NASCADE_ZIP"]).extractall(".")'
chmod +x "$STAGE/Nascade.pak/nascade" "$STAGE/Nascade.pak/launch.sh"
cd "$STAGE/Nascade.pak"
export LD_LIBRARY_PATH="$PWD/lib:$LIBC/lib:$T"
qemu-aarch64-static -L "$LIBC" ./nascade --help > /tmp/nascade_help.txt
echo HELP_EXIT=$?
head -n 2 /tmp/nascade_help.txt

export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_LOCAL_NAS="$ROOT/devdata/NasRoot"
export NASCADE_REMOTE_PATH='GBA/NAS Test Game.gba'
export NASCADE_LOCAL_PATH="$ROOT/devdata/Roms/GBA/NAS Test Game Packaged.gba"
export NASCADE_REMOTE_SIZE=131072
rm -f "$NASCADE_LOCAL_PATH" "$NASCADE_LOCAL_PATH.part"
qemu-aarch64-static -L "$LIBC" ./nascade --test-download
echo DL_EXIT=$?
sha1sum "$ROOT/devdata/NasRoot/GBA/NAS Test Game.gba" "$NASCADE_LOCAL_PATH"
