#!/bin/sh
# 在 qemu 下用本地模拟 ScreenScraper 测试刮削链路。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
BIN="$CROSS_DIR/build-nascade/bin/nascade"
LIBC="$CROSS_DIR/toolchain/aarch64-linux-gnu/libc"
LIBSTD="$CROSS_DIR/toolchain/aarch64-linux-gnu/lib64"
SYS="$CROSS_DIR/sysroot/usr/lib"

export LD_LIBRARY_PATH="$LIBC/lib:$LIBSTD:$SYS"
export NASCADE_SD_ROOT="$ROOT/devdata"
export NASCADE_SS_DEVID=test
export NASCADE_SS_DEVPASSWORD=test
export NASCADE_SS_BASEURL=http://127.0.0.1:8765
export NO_PROXY=127.0.0.1,localhost

cd "$ROOT"
python3 tools/mock_screenscraper.py 8765 >/tmp/mock_scraper.log 2>&1 &
MOCK=$!
sleep 1
trap 'kill $MOCK 2>/dev/null || true' EXIT

echo '--- target test-scraper ---'
qemu-aarch64-static -L "$LIBC" "$BIN" --test-scraper
echo '--- target test-scrape ---'
qemu-aarch64-static -L "$LIBC" "$BIN" --test-scrape | head -n 10
