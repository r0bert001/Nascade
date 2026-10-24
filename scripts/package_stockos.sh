#!/bin/sh
# 组装原厂 TrimUI OS 的 Apps/Nascade.pak 发布包。
# 用法: scripts/package_stockos.sh [包含 nascade 可执行文件的构建目录]
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${1:-"$ROOT/build/tg5040"}
PAK_NAME=Nascade.pak
OUT="$ROOT/build/$PAK_NAME"

if [ ! -x "$BUILD_DIR/nascade" ]; then
    echo "找不到 aarch64 目标程序: $BUILD_DIR/nascade" >&2
    echo "请先用官方 SDK 交叉编译，或把构建目录作为第一个参数传入。" >&2
    exit 1
fi

if [ ! -f "$ROOT/assets/fonts/NotoSansCJKsc-Regular.otf" ]; then
    echo "缺少中文字体，请先运行 scripts/fetch_assets.sh" >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/assets/fonts" "$OUT/lib"

cp "$ROOT/packaging/stockos/$PAK_NAME/config.json" "$OUT/"
cp "$ROOT/packaging/stockos/$PAK_NAME/config.example.json" "$OUT/"
cp "$ROOT/packaging/stockos/$PAK_NAME/icon.png" "$OUT/"
cp "$ROOT/packaging/stockos/$PAK_NAME/launch.sh" "$OUT/"
cp "$BUILD_DIR/nascade" "$OUT/"
cp "$ROOT/assets/fonts/NotoSansCJKsc-Regular.otf" "$OUT/assets/fonts/"
cp "$ROOT/assets/cacert.pem" "$OUT/assets/"

if [ -d "$BUILD_DIR/lib" ]; then
    cp -a "$BUILD_DIR/lib/." "$OUT/lib/"
fi

chmod +x "$OUT/launch.sh" "$OUT/nascade"

cd "$ROOT/build"
rm -f "$PAK_NAME.zip"
if command -v zip >/dev/null 2>&1; then
    zip -r "$PAK_NAME.zip" "$PAK_NAME" >/dev/null
else
    python3 -m zipfile -c "$PAK_NAME.zip" "$PAK_NAME"
fi

echo "已生成: $ROOT/build/$PAK_NAME.zip"
