#!/bin/sh
echo '=== show.json ==='
cat /mnt/SDCARD/Apps/show.json 2>/dev/null | head -n 120
echo '=== SDL2 search ==='
find /usr /lib /opt -maxdepth 4 -name 'libSDL2*' 2>/dev/null | head -n 20
echo '=== /tmp space ==='
df -h /tmp /mnt/SDCARD 2>/dev/null
echo '=== mounts ==='
cat /proc/mounts | grep -E 'tmp|SDCARD' | head -n 20
echo '=== runtrimui ==='
cat /usr/trimui/bin/runtrimui.sh 2>/dev/null | head -n 120
echo '=== trimui dirs ==='
ls -la /usr/trimui 2>/dev/null | head -n 40
echo '=== app menu refs ==='
grep -RIl 'Apps' /usr/trimui 2>/dev/null | head -n 20
