#!/bin/sh
# Nascade 原厂 TrimUI OS 启动脚本
# 应用目录: /mnt/SDCARD/Apps/Nascade.pak/
cd "$(dirname "$0")" || exit 1
chmod +x ./nascade 2>/dev/null || true

if [ -d /usr/trimui/lib ]; then
    export LD_LIBRARY_PATH="/usr/trimui/lib:$PWD/lib:$LD_LIBRARY_PATH"
else
    export LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
fi
mkdir -p /mnt/SDCARD/.Nascade/logs 2>/dev/null

./nascade "$@" >> /mnt/SDCARD/.Nascade/logs/launch.log 2>&1
