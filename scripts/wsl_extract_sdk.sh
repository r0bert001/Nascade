#!/bin/sh
# 解压官方 TrimUI SDK、Linaro 工具链和 SDL2 源码。
set -eu

CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
SDK_DOWNLOAD_DIR=${SDK_DOWNLOAD_DIR:-$HOME/tg5040-sdk-downloads}

mkdir -p "$CROSS_DIR/toolchain" "$CROSS_DIR/sysroot" "$CROSS_DIR/sdl2-src" "$CROSS_DIR/deps"
cd "$SDK_DOWNLOAD_DIR"
tar -xzf aarch64-linux-gnu-7.5.0-linaro.tgz -C "$CROSS_DIR/toolchain" --strip-components=1
tar -xzf SDK_usr_tg5040_a133p.tgz -C "$CROSS_DIR/sysroot"
tar -xzf SDL2-2.26.1.GE8300.tgz -C "$CROSS_DIR/sdl2-src" --strip-components=1

echo "--- compiler ---"
"$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-gcc" --version | head -n 1
"$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-g++" --version | head -n 1
echo "--- sysroot ---"
"$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-gcc" -print-sysroot
echo "--- hello ---"
cat > /tmp/nascade_hello.c <<'C'
#include <stdio.h>
int main(void) { puts("hello tg5040"); return 0; }
C
"$CROSS_DIR/toolchain/bin/aarch64-linux-gnu-gcc" -o /tmp/nascade_hello /tmp/nascade_hello.c
file /tmp/nascade_hello
echo "--- sdk libs ---"
ls "$CROSS_DIR"/sysroot/usr/lib/libSDL2*.so* "$CROSS_DIR"/sysroot/usr/lib/libSDL2_ttf*.so* "$CROSS_DIR"/sysroot/usr/lib/libsqlite3.so* "$CROSS_DIR"/sysroot/usr/lib/libssl.so* "$CROSS_DIR"/sysroot/usr/lib/libcrypto.so* 2>/dev/null | head -n 30
