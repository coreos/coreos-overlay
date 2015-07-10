#!/bin/bash
set -e

API_URL=$(curl -s https://metadata.packet.net/metadata | jq ".api_url" | sed -e 's/^"//'  -e 's/"$//')

while ! wget --inet4-only --spider --quiet "${API_URL}/build_info.txt"; do
	echo "$0: API unavailable: $API_URL" >&2
	sleep 2
done

logger "Making a call to tell the packet API this machine is online."
curl -X POST $API_URL/phone-home
logger "This device has been announced to the packet API."
