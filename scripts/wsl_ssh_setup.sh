#!/bin/sh
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get install -y --no-install-recommends sshpass openssh-client >/tmp/sshpass_install.log 2>&1
echo SSH_READY
