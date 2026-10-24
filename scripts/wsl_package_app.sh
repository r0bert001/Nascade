#!/bin/sh
# 从构建目录收集二进制和运行库，并生成安装包。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-/opt/tg5040/build-nascade}
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
DEST="$ROOT/build/tg5040"

rm -rf "$DEST"
mkdir -p "$DEST/lib"
cp "$BUILD_DIR/bin/nascade" "$DEST/nascade"
S="$CROSS_DIR/sysroot/usr/lib"
T="$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
cp -L "$S/libSDL2-2.0.so.0.2600.1" "$DEST/lib/libSDL2-2.0.so.0"
cp -L "$S/libSDL2_ttf-2.0.so.0.10.3" "$DEST/lib/libSDL2_ttf-2.0.so.0"
cp -L "$S/libfreetype.so.6.12.1" "$DEST/lib/libfreetype.so.6"
cp -L "$S/libz.so.1.2.8" "$DEST/lib/libz.so.1"
cp -L "$S/libbz2.so.1.0.6" "$DEST/lib/libbz2.so.1.0"
cp -L "$T/libstdc++.so.6.0.24" "$DEST/lib/libstdc++.so.6"
cp -L "$T/libgcc_s.so.1" "$DEST/lib/libgcc_s.so.1"
chmod +x "$DEST/nascade"

cd "$ROOT"
sh scripts/package_stockos.sh "$DEST"
echo '--- package ---'
ls -lh "$ROOT/build/Nascade.pak.zip"
NASCADE_ZIP="$ROOT/build/Nascade.pak.zip" python3 -c 'import os, zipfile; z=zipfile.ZipFile(os.environ["NASCADE_ZIP"]); [print(i.filename, i.file_size) for i in z.infolist()]' | head -n 30
