#!/bin/bash

/usr/bin/block-until-url http://169.254.169.254/

USER_DIR="/home/core/user"

if [ ! -d ${USER_DIR}/.ssh ] ; then
	mkdir -p ${USER_DIR}/.ssh
	chmod 700 ${USER_DIR}/.ssh
fi
# Fetch public key using HTTP
curl -s --connect-timeout 3 http://169.254.169.254/latest/meta-data/public-keys/0/openssh-key > /tmp/my-key
if [ $? -eq 0 ] ; then
	cat /tmp/my-key >> ${USER_DIR}/.ssh/authorized_keys
	chmod 700 ${USER_DIR}/.ssh/authorized_keys
	rm /tmp/my-key
else
	echo unable to download key
	rm /tmp/my-key
	exit 1
fi
chown -R core: $USER_DIR/.ssh
