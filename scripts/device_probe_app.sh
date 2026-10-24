#!/bin/sh
# 在 Brick Pro 上运行的临时探测脚本；由 device_deploy_tmp.sh 上传的包已解压后使用。
# 用法（在设备上）: PAK=/tmp/nascade_test/Nascade.pak NASCADE_SD_ROOT=/tmp/nascade_sd sh device_probe_app.sh
set -u

PAK=${PAK:-/tmp/nascade_test/Nascade.pak}
SD_ROOT=${NASCADE_SD_ROOT:-/tmp/nascade_sd}
cd "$PAK" || exit 1
export NASCADE_SD_ROOT="$SD_ROOT"
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy

echo '=== help bundled libs ==='
LD_LIBRARY_PATH="$PAK/lib:/usr/trimui/lib" ./nascade --help > /tmp/help_bundled.txt 2>&1
echo HELP_BUNDLED_EXIT=$?
head -n 3 /tmp/help_bundled.txt

echo '=== help stock SDL2 ==='
LD_LIBRARY_PATH="/usr/trimui/lib:$PAK/lib" ./nascade --help > /tmp/help_stock.txt 2>&1
echo HELP_STOCK_EXIT=$?
head -n 3 /tmp/help_stock.txt

echo '=== local download test ==='
rm -rf /tmp/nascade_nas /tmp/nascade_out
mkdir -p /tmp/nascade_nas/GBA /tmp/nascade_out
dd if=/dev/zero of=/tmp/nascade_nas/GBA/test.gba bs=1024 count=64 2>/dev/null
NASCADE_LOCAL_NAS=/tmp/nascade_nas NASCADE_REMOTE_PATH='GBA/test.gba' \
NASCADE_LOCAL_PATH=/tmp/nascade_out/test.gba NASCADE_REMOTE_SIZE=65536 \
LD_LIBRARY_PATH="$PAK/lib:/usr/trimui/lib" ./nascade --test-download
echo DL_EXIT=$?
ls -l /tmp/nascade_out/test.gba 2>/dev/null
md5sum /tmp/nascade_nas/GBA/test.gba /tmp/nascade_out/test.gba 2>/dev/null

echo '=== screenshot bundled SDL2 ==='
rm -f /tmp/nascade_home.ppm
LD_LIBRARY_PATH="$PAK/lib:/usr/trimui/lib" ./nascade --screenshot /tmp/nascade_home.ppm --screen home
echo SHOT_BUNDLED_EXIT=$?
ls -l /tmp/nascade_home.ppm 2>/dev/null

echo '=== screenshot stock SDL2 ==='
rm -f /tmp/nascade_home_stock.ppm
LD_LIBRARY_PATH="/usr/trimui/lib:$PAK/lib" ./nascade --screenshot /tmp/nascade_home_stock.ppm --screen home
echo SHOT_STOCK_EXIT=$?
ls -l /tmp/nascade_home_stock.ppm 2>/dev/null

echo '=== logs ==='
tail -n 30 "$SD_ROOT/.Nascade/logs/Nascade.log" 2>/dev/null
