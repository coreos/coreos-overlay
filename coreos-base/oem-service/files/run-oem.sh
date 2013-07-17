#!/bin/bash

# Each oem implemenation has a run.sh that boostraps its OEM process for systemd
OEM_EXEC="/usr/share/oem/run.sh"

[ -e $OEM_EXEC ] && exec $OEM_EXEC || exit 0
