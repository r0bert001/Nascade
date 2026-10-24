#!/bin/sh
# 从 Brick Pro 取回离屏渲染截图并转成 PNG。
# 用法: BRICK_HOST=<设备 IP> sh scripts/device_fetch_shot.sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BRICK_HOST=${BRICK_HOST:?请设置 BRICK_HOST，例如 BRICK_HOST=192.168.1.50}

sshpass -e ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -o ConnectTimeout=10 root@"$BRICK_HOST" 'cat /tmp/nascade_home.ppm' > "$ROOT/screenshots/device_home.ppm"
ls -lh "$ROOT/screenshots/device_home.ppm"
python3 "$ROOT/tools/ppm_to_png.py" "$ROOT/screenshots/device_home.ppm" "$ROOT/screenshots/device_home.png"
