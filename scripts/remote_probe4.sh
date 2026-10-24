#!/bin/sh
echo '=== show.json ==='
cat /mnt/SDCARD/Apps/show.json 2>/dev/null
echo '=== trimui lib ==='
ls -l /usr/trimui/lib 2>/dev/null | head -n 40
echo '=== SDL2 find ==='
find /usr/trimui /usr/lib /lib -maxdepth 3 -iname 'libSDL2*' 2>/dev/null | head -n 20
echo '=== trimui apps ==='
ls -la /usr/trimui/apps 2>/dev/null | head -n 30
echo '=== app config example ==='
cat /mnt/SDCARD/Apps/File/config.json 2>/dev/null
echo '=== runtrimui tail ==='
tail -n 80 /usr/trimui/bin/runtrimui.sh 2>/dev/null
