#!/bin/bash

set -e

USER_DIR="/home/core"

if [ -e "${USER_DIR}/.ssh/authorized_keys" ]; then
    exit 0
fi

if [ ! -d "${USER_DIR}/.ssh" ]; then
	mkdir -p ${USER_DIR}/.ssh
	chmod 700 ${USER_DIR}/.ssh
fi

cp /usr/share/oem/vagrant.pub "${USER_DIR}/.ssh/authorized_keys"
chown -R core: "${USER_DIR}/.ssh"
