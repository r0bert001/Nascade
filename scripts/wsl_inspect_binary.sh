#!/bin/sh
set -e
BIN=/opt/tg5040/build-nascade/bin/nascade
TC=/opt/tg5040/toolchain/bin
echo "--- size ---"
ls -lh "$BIN"
echo "--- NEEDED ---"
"$TC/aarch64-linux-gnu-readelf" -d "$BIN" | grep NEEDED
echo "--- GLIBC versions ---"
"$TC/aarch64-linux-gnu-readelf" --version-info "$BIN" | grep -oE 'GLIBC_[0-9.]+' | sort -V | uniq | tail -n 10
echo "--- RPATH/RUNPATH ---"
"$TC/aarch64-linux-gnu-readelf" -d "$BIN" | grep -E 'RPATH|RUNPATH' || true
echo "--- unresolved? ---"
"$TC/aarch64-linux-gnu-readelf" -s "$BIN" | grep 'UND' | head -n 10 || true
