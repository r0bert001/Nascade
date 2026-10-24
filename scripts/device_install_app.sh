#!/bin/sh
# 打包并安装到 Brick Pro 的 /mnt/SDCARD/Apps/Nascade.pak。
# 用法: BRICK_HOST=<设备 IP> sh scripts/device_install_app.sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BRICK_HOST=${BRICK_HOST:?请设置 BRICK_HOST，例如 BRICK_HOST=192.168.1.50}

cd "$ROOT"
sh scripts/package_stockos.sh "$ROOT/build/tg5040"
mkdir -p dist
cp build/Nascade.pak.zip dist/Nascade.pak.zip

cd build
tar -czf /tmp/Nascade.pak.install.tar.gz Nascade.pak
sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 root@"$BRICK_HOST" 'set -e; if [ -e /mnt/SDCARD/Apps/Nascade.pak ]; then echo ERR_EXISTS; exit 2; fi; rm -rf /tmp/nascade_install; mkdir -p /tmp/nascade_install; cat > /tmp/Nascade.pak.install.tar.gz; tar -xzf /tmp/Nascade.pak.install.tar.gz -C /tmp/nascade_install; ls -la /tmp/nascade_install/Nascade.pak | head -n 20; df -h /mnt/SDCARD; cp -a /tmp/nascade_install/Nascade.pak /mnt/SDCARD/Apps/; sync; echo INSTALLED; ls -la /mnt/SDCARD/Apps/Nascade.pak | head -n 20; df -h /mnt/SDCARD' < /tmp/Nascade.pak.install.tar.gz
