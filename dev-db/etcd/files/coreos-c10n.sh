#!/bin/bash

C10N_ENDPOINT=https://core-api.appspot.com/v1/c10n/group
META_URL="http://169.254.169.254/latest"

USER_DATA=$(curl -s $META_URL/user-data)

URL=$USER_DATA

echo $URL | grep -q '^https://' || (echo Coordination URL requires valid SSL; exit 1)

TMP=`mktemp`

curl -s "$USER_DATA/raw" > $TMP

# validate ssh key
ssh-keygen -l -f $TMP > /dev/null 2>&1
if [ $? -eq 0 ]; then
        cat $TMP >> $HOME/.ssh/authorized_keys
        echo "SSH key updated"
else
        echo "Not a valid ssh key"
fi

IP_LIST=""
for IP4 in `curl -s $META_URL/meta-data/ | grep ipv4`; do
        IP=$(curl -s $META_URL/meta-data/$IP4)
        if [ "$IP_LIST" != "" ]; then
                IP_LIST="$IP_LIST,$IP"
        else
                IP_LIST="$IP"
        fi
done

curl $C10N_ENDPOINT -d "c10n_url=$URL" -d"ip_list=$IP_LIST"
