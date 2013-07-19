#!/bin/bash

OEM_DIR="/usr/share/oem"
/usr/bin/systemd-nspawn -D $OEM_DIR/nova-agent/ /bin/sh -c      \
    "HOME=/root mount -t xenfs none /proc/xen;    \
    /usr/share/nova-agent/0.0.1.37/sbin/nova-agent \
    -o - -n -l info /usr/share/nova-agent/nova-agent.py resetnetwork" &

NET_CONF=$OEM_DIR/nova-agent/etc/conf.d/net
while [ ! -e ${NET_CONF} ]; do
        echo waiting...
        sleep .1
done

. ${NET_CONF}
. ${OEM_DIR}/usr/bin/ifconfig.sh

for if in eth0 eth1; do
        IFACE=$if
        OLDIFS=$IFS
        IFS=$'\n'
        CONFIG="config_${if}"
        eval CONFIG=\$$CONFIG
        for net in $CONFIG; do
                IFS=$OLDIFS
                _add_address $net
        done
        OLDIFS=$IFS
        IFS=$'\n'
        ROUTE="routes_${if}"
        eval ROUTE=\$$ROUTE
        for route in $ROUTE; do
                IFS=$OLDIFS
                #_add_route $route
                ip route add $route dev $if
        done
done
for ns in $dns_servers_eth0; do
        echo nameserver $ns >> /var/run/resolv.conf
done

