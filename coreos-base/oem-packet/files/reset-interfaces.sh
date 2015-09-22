#!/bin/bash
set -e
logger "Hard reset of all network interfaces due to upstream systemd-networkd issue: https://github.com/coreos/bugs/issues/36 ."
for iface in `ls -1 /sys/class/net | egrep -v "(^lo$|^bond)"`; do
	ip link set $iface down
done
ip link del bond0
systemctl restart systemd-networkd
