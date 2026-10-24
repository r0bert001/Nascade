#!/bin/sh
echo '=== libc ==='
ls -l /lib/libc.so.6 /lib/ld-linux-aarch64.so.1 2>/dev/null
/lib/libc.so.6 2>/dev/null | head -n 2
/lib/ld-linux-aarch64.so.1 --version 2>/dev/null | head -n 2
echo '=== sdl libs ==='
find /usr/lib /lib -maxdepth 2 \( -name 'libSDL2*' -o -name 'libSDL2_ttf*' \) 2>/dev/null | head -n 20
ls -l /usr/lib/libstdc++.so.6 /lib/libstdc++.so.6 2>/dev/null
echo '=== apps ==='
ls -la /mnt/SDCARD/Apps 2>/dev/null | head -n 30
echo '=== brick config ==='
cat /mnt/SDCARD/Apps/BrickTools/config.json 2>/dev/null
echo '=== df sd ==='
df -h /mnt/SDCARD 2>/dev/null
echo '=== version files ==='
find /etc /usr/etc -maxdepth 2 -iname '*version*' 2>/dev/null | head -n 20
for f in /etc/os-release /etc/version /etc/trimui_version /etc/issue; do [ -f "$f" ] && { echo "--- $f"; head -n 10 "$f"; }; done
echo '=== ui proc ==='
ps 2>/dev/null | grep -iE 'trimui|frontend|launcher|main|ui' | head -n 20
echo '=== sd writable test (read-only stat) ==='
ls -ld /mnt/SDCARD /mnt/SDCARD/Apps /mnt/SDCARD/Roms
