#!/bin/bash

AGENT_ROOT="/usr/share/oem/nova-agent"
/bin/mount --bind /proc ${AGENT_ROOT}/proc
/bin/mount -t xenfs none ${AGENT_ROOT}/proc/xen
/bin/mount --bind /dev ${AGENT_ROOT}/dev
/bin/mount --bind /sys ${AGENT_ROOT}/sys
/usr/bin/chroot /usr/share/oem/nova-agent/ /bin/sh -c "HOME=/root /usr/share/nova-agent/0.0.1.38/sbin/nova-agent -o - -n -l info /usr/share/nova-agent/nova-agent.py" 
/bin/umount ${AGENT_ROOT}/proc/xen
/bin/umount ${AGENT_ROOT}/proc
/bin/umount ${AGENT_ROOT}/dev
/bin/umount ${AGENT_ROOT}/sys
