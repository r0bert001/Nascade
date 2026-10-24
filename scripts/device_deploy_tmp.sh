#!/bin/sh
# 把已打包的 Nascade 上传到 Brick Pro 的 /tmp，用于临时调试。
# 用法: BRICK_HOST=<设备 IP> sh scripts/device_deploy_tmp.sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BRICK_HOST=${BRICK_HOST:?请设置 BRICK_HOST，例如 BRICK_HOST=192.168.1.50}

cd "$ROOT/build"
tar -czf /tmp/Nascade.pak.tar.gz Nascade.pak
ls -lh /tmp/Nascade.pak.tar.gz

sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 root@"$BRICK_HOST" 'cat > /tmp/Nascade.pak.tar.gz' < /tmp/Nascade.pak.tar.gz

sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 root@"$BRICK_HOST" 'rm -rf /tmp/nascade_test /tmp/nascade_sd; mkdir -p /tmp/nascade_test /tmp/nascade_sd; tar -xzf /tmp/Nascade.pak.tar.gz -C /tmp/nascade_test; ln -s /mnt/SDCARD/Emus /tmp/nascade_sd/Emus; ln -s /mnt/SDCARD/Roms /tmp/nascade_sd/Roms; ln -s /mnt/SDCARD/Imgs /tmp/nascade_sd/Imgs; chmod +x /tmp/nascade_test/Nascade.pak/nascade; ls -la /tmp/nascade_test/Nascade.pak | head -n 20; df -h /tmp'
