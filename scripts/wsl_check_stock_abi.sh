#!/bin/sh
BIN="/mnt/e/Apps应用包/Brick工具箱 20251108 新增截图功能/Apps/BrickTools/scripts/bin/trimui_inputd"
R=/opt/tg5040/toolchain/bin/aarch64-linux-gnu-readelf
echo "--- file ---"
file "$BIN"
echo "--- NEEDED ---"
$R -d "$BIN" | grep NEEDED || true
echo "--- GLIBC max ---"
$R --version-info "$BIN" | grep -oE 'GLIBC_[0-9.]+' | sort -V | uniq | tail -n 12
