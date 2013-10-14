#!/bin/bash

OEM_DIR="/usr/share/oem"
SSH_CONF=$OEM_DIR/nova-agent/root/.ssh/authorized_keys
while [ ! -e ${SSH_CONF} ]; do
        echo waiting...
        sleep .1
done

/bin/grep ssh- ${SSH_CONF}  | /usr/bin/update-ssh-keys -u core 
