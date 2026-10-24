#!/bin/sh
# 在 Windows Git Bash 中调用便携 MSYS2 构建桌面调试版。
# 依赖：MSYS2 mingw64 工具链 + 已构建的 libsmb2。
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT_WIN=$(cd "$ROOT" && pwd -W 2>/dev/null || printf '%s' "$ROOT")

MSYS2_ROOT=${MSYS2_ROOT:-E:/code/tools/msys64}
LIBSMB2_SRC=${LIBSMB2_SRC:-E:/code/ref/libsmb2_src}
LIBSMB2_BUILD=${LIBSMB2_BUILD:-E:/code/ref/libsmb2_build}

if [ ! -x "$MSYS2_ROOT/usr/bin/bash.exe" ]; then
    echo "找不到 MSYS2: $MSYS2_ROOT（可用 MSYS2_ROOT 覆盖）" >&2
    exit 1
fi

"$MSYS2_ROOT/usr/bin/bash.exe" -lc "
set -eu
export MSYSTEM=MINGW64
export PATH=/mingw64/bin:\$PATH
cd '$ROOT_WIN'
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DNASCADE_LIBSMB2_INCLUDE_DIR='$LIBSMB2_SRC/include' \
  -DNASCADE_LIBSMB2_BUILD_INCLUDE_DIR='$LIBSMB2_BUILD' \
  -DNASCADE_LIBSMB2_LIBRARY='$LIBSMB2_BUILD/lib/libsmb2.a'
cmake --build build/mingw
"

echo "构建完成：$ROOT/build/mingw/bin/nascade.exe"
