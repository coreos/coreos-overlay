#!/bin/bash

OEM_DIR="/usr/share/oem"
SSH_CONF=$OEM_DIR/nova-agent/root/.ssh/authorized_keys
while [ ! -e ${SSH_CONF} ]; do
        echo waiting...
        sleep .1
done

/usr/bin/update-ssh-keys -a nova-agent /usr/share/oem/nova-agent/root/.ssh/authorized_keys
