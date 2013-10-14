#!/bin/bash

OEM_DIR="/usr/share/oem"
CONF=$OEM_DIR/nova-agent/etc/resolv.conf
while [ ! -e ${CONF} ]; do
        echo waiting...
        sleep .1
done

cp ${CONF} /etc/resolv.conf
