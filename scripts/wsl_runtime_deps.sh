#!/bin/sh
set -e
S=/opt/tg5040/sysroot/usr/lib
T=/opt/tg5040/toolchain
R=$T/bin/aarch64-linux-gnu-readelf
for f in "$S/libSDL2-2.0.so.0" "$S/libSDL2_ttf-2.0.so.0" "$S/libfreetype.so.6"; do
  echo "=== $f"
  ls -l "$f"
  $R -d "$f" | grep NEEDED || true
done
echo '=== find libstdc++/libgcc'
find "$T" -name 'libstdc++.so.6*' -o -name 'libgcc_s.so.1' | head -n 20
echo '=== toolchain libc'
ls -l "$T/aarch64-linux-gnu/libc/lib/ld-linux-aarch64.so.1" "$T/aarch64-linux-gnu/libc/lib/libc.so.6" 2>/dev/null || true
