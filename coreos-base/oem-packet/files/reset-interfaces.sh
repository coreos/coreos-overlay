#!/bin/bash
set -e

logger "Hard reset of all network interfaces due to upstream systemd-networkd issue: https://github.com/coreos/bugs/issues/36 ."
ip link set enp1s0f0 down
ip link set enp1s0f1 down
ip link del bond0
systemctl restart systemd-networkd
