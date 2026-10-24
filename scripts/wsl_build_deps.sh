#!/bin/sh
# 交叉编译 libcurl 和 libsmb2 静态库。
set -eu

CROSS_DIR=${CROSS_DIR:-/opt/tg5040}
TOOLCHAIN="$CROSS_DIR/toolchain"
SYSROOT="$CROSS_DIR/sysroot"
DEPS="$CROSS_DIR/deps"
CURL_ARCHIVE=${CURL_ARCHIVE:-$HOME/tg5040-downloads/curl-8.11.1.tar.xz}
LIBSMB2_SRC=${LIBSMB2_SRC:-$HOME/libsmb2}

if [ ! -f "$CURL_ARCHIVE" ]; then
    echo "找不到 libcurl 源码包: $CURL_ARCHIVE（可用 CURL_ARCHIVE 覆盖）" >&2
    exit 1
fi
if [ ! -d "$LIBSMB2_SRC" ]; then
    echo "找不到 libsmb2 源码目录: $LIBSMB2_SRC（可用 LIBSMB2_SRC 覆盖）" >&2
    exit 1
fi

export CC="$TOOLCHAIN/bin/aarch64-linux-gnu-gcc"
export CXX="$TOOLCHAIN/bin/aarch64-linux-gnu-g++"
export AR="$TOOLCHAIN/bin/aarch64-linux-gnu-ar"
export RANLIB="$TOOLCHAIN/bin/aarch64-linux-gnu-ranlib"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export CFLAGS="-O2 -I$SYSROOT/usr/include"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$SYSROOT/usr/lib -Wl,-rpath-link,$SYSROOT/usr/lib"

mkdir -p "$CROSS_DIR/build" "$DEPS"
cd "$CROSS_DIR/build"
rm -rf curl-8.11.1
tar -xf "$CURL_ARCHIVE"
cd curl-8.11.1

./configure \
  --host=aarch64-linux-gnu \
  --prefix="$DEPS" \
  --with-openssl="$SYSROOT/usr" \
  --with-zlib="$SYSROOT/usr" \
  --disable-shared --enable-static \
  --disable-ldap --disable-ldaps \
  --without-libpsl --without-libidn2 --without-brotli --without-zstd \
  --disable-ftp --disable-file --disable-dict --disable-telnet --disable-tftp \
  --disable-pop3 --disable-imap --disable-smtp --disable-gopher --disable-mqtt \
  --disable-manual --disable-docs

make -j4
make install

echo "=== curl-config static ==="
"$DEPS/bin/curl-config" --static-libs || true

rm -rf "$CROSS_DIR/build-libsmb2"
cmake -S "$LIBSMB2_SRC" -B "$CROSS_DIR/build-libsmb2" \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_INSTALL_PREFIX="$DEPS" \
  -DBUILD_SHARED_LIBS=OFF \
  -DENABLE_LIBKRB5=OFF -DENABLE_GSSAPI=OFF -DENABLE_LIBDCERPC=OFF \
  -DENABLE_EXAMPLES=OFF -DENABLE_UTILS=OFF
cmake --build "$CROSS_DIR/build-libsmb2" -j4
cmake --install "$CROSS_DIR/build-libsmb2"

echo "=== installed ==="
find "$DEPS" -maxdepth 3 -type f | sort | head -n 60
file "$DEPS/lib/libcurl.a" "$DEPS/lib/libsmb2.a"
