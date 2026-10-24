#!/bin/sh
# 下载运行所需的中文字体与 CA 证书。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
FONT_DIR="$ROOT/assets/fonts"
FONT="$FONT_DIR/NotoSansCJKsc-Regular.otf"
CA="$ROOT/assets/cacert.pem"

mkdir -p "$FONT_DIR"

if [ ! -f "$FONT" ]; then
    echo "下载 Noto Sans CJK SC..."
    curl -L --fail --retry 3 --connect-timeout 20 \
        -o "$FONT.tmp" \
        "https://cdn.jsdelivr.net/gh/googlefonts/noto-cjk@main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf"
    mv "$FONT.tmp" "$FONT"
fi

if [ ! -f "$CA" ]; then
    echo "下载 CA 证书包..."
    curl -L --fail --retry 3 --connect-timeout 20 \
        -o "$CA.tmp" \
        "https://curl.se/ca/cacert.pem"
    mv "$CA.tmp" "$CA"
fi

echo "资源已就绪："
ls -lh "$FONT" "$CA"
