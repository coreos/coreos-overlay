#! /bin/bash

IFINDEX=$1
echo "ID_NET_NAME_SIMPLE=eth$((${IFINDEX} - 2))"
