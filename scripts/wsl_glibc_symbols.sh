#!/bin/sh
BIN=/opt/tg5040/build-nascade/bin/nascade
/opt/tg5040/toolchain/bin/aarch64-linux-gnu-readelf -Ws "$BIN" | grep 'GLIBC_2.25' | head -n 20 || true
