#!/bin/sh
# 在 WSL 中交叉编译 aarch64 目标版本。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-/opt/tg5040/build-nascade}
CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
TOOLCHAIN="$CROSS_DIR/toolchain"

rm -rf "$BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/tg5040-toolchain.cmake" \
  -DNASCADE_CROSS=ON \
  -DNASCADE_SYSROOT="$CROSS_DIR/sysroot" \
  -DNASCADE_DEPS="$CROSS_DIR/deps" \
  -DNASCADE_LIBSMB2_INCLUDE_DIR="$CROSS_DIR/deps/include" \
  -DNASCADE_LIBSMB2_LIBRARY="$CROSS_DIR/deps/lib/libsmb2.a" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j4
file "$BUILD_DIR/bin/nascade"
"$TOOLCHAIN/bin/aarch64-linux-gnu-readelf" -d "$BUILD_DIR/bin/nascade" | grep NEEDED || true
