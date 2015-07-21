#!/bin/bash
logger "Hard reset of all network interfaces due to upstream systemd-networkd issue: https://github.com/coreos/bugs/issues/36 ."
for iface in `cat /proc/net/dev | awk {'print $1'} | grep ":" | sed -e 's/://' | egrep -v "(lo|bond)"`; do
        ip link set $iface down
done
ip link del bond0
systemctl restart systemd-networkd
sleep 15
