#! /bin/bash

INTERFACES=$(ip link show | gawk -F ':' '/^[0-9]+/ { print $2 }' | tr -d ' ' | sed 's/lo//')
for iface in ${INTERFACES}; do
	ip link set ${iface} down
	udevadm test /sys/class/net/${iface}
done
