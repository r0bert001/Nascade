#!/bin/sh
set -e
export PKG_CONFIG_SYSROOT_DIR=/opt/tg5040/sysroot
export PKG_CONFIG_LIBDIR=/opt/tg5040/sysroot/usr/lib/pkgconfig:/opt/tg5040/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:/opt/tg5040/sysroot/usr/share/pkgconfig
echo "--- pc files ---"
ls /opt/tg5040/sysroot/usr/lib/pkgconfig | sort | head -n 60
for m in sdl2 SDL2_ttf sqlite3 openssl libssl libcrypto freetype2; do
  echo "=== $m ==="
  pkg-config --modversion "$m" 2>&1 || true
  pkg-config --cflags --libs "$m" 2>&1 || true
done
echo "--- libs ---"
ls -l /opt/tg5040/sysroot/usr/lib/libSDL2.so /opt/tg5040/sysroot/usr/lib/libSDL2_ttf.so /opt/tg5040/sysroot/usr/lib/libsqlite3.so /opt/tg5040/sysroot/usr/lib/libssl.so /opt/tg5040/sysroot/usr/lib/libcrypto.so /opt/tg5040/sysroot/usr/lib/libz.so
