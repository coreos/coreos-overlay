#!/bin/bash

OEM_DIR="/usr/share/oem"
SSH_CONF=$OEM_DIR/nova-agent/root/.ssh/authorized_keys
while [ ! -e ${SSH_CONF} ]; do
        echo waiting...
        sleep .1
done

SSH_HOME="/home/core/.ssh"
SSH_FILE="${SSH_HOME}/authorized_keys"
mkdir -p ${SSH_HOME}
cp ${SSH_CONF} ${SSH_FILE}
chown -R core:core ${SSH_HOME}

