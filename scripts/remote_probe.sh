#!/bin/sh
echo '=== uname ==='
uname -a
echo '=== version files ==='
for f in /etc/os-release /etc/version /etc/version.txt /etc/issue /etc/trimui_version /etc/build; do
  if [ -f "$f" ]; then echo "--- $f"; cat "$f"; fi
done
echo '=== libc ==='
ls -l /lib/libc.so.6 /lib/ld-linux-aarch64.so.1 2>/dev/null
/lib/libc.so.6 2>/dev/null | head -n 2 || true
/lib/ld-linux-aarch64.so.1 --version 2>/dev/null | head -n 2 || true
echo '=== sdl/other libs ==='
ls -l /usr/lib/libSDL2* /usr/lib/libSDL2_ttf* 2>/dev/null
ls /usr/lib 2>/dev/null | grep -iE 'curl|ssl|crypto|sqlite|freetype|stdc' | head -n 30
echo '=== sd mount ==='
mount | grep -E 'mmc|SDCARD' | head -n 10
df -h | grep -E 'mmc|SDCARD' | head -n 10
echo '=== sd dirs ==='
ls -la /mnt/SDCARD 2>/dev/null | head -n 30
echo '--- Apps'
ls -la /mnt/SDCARD/Apps 2>/dev/null | head -n 40
echo '--- BrickTools config'
cat /mnt/SDCARD/Apps/BrickTools/config.json 2>/dev/null | head -n 20
echo '--- Emus'
ls -la /mnt/SDCARD/Emus 2>/dev/null | head -n 40
echo '--- Roms'
ls -la /mnt/SDCARD/Roms 2>/dev/null | head -n 40
echo '--- Imgs'
ls -la /mnt/SDCARD/Imgs 2>/dev/null | head -n 20
echo '=== tools ==='
for t in wget curl tar unzip zip scp sftp python3 python; do command -v "$t" 2>/dev/null; done
echo '=== display/input ==='
ls -l /dev/fb* 2>/dev/null | head -n 10
ls -l /dev/dri 2>/dev/null | head -n 10
ls -l /dev/input 2>/dev/null | head -n 20
cat /sys/class/graphics/fb0/virtual_size 2>/dev/null || true
echo '=== system ==='
nproc
free -m 2>/dev/null | head -n 4
cat /proc/cmdline
echo '=== processes ==='
ps 2>/dev/null | head -n 25
