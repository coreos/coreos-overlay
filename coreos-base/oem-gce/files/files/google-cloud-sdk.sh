#!/bin/sh
alias gcloud="(docker images google/cloud-sdk || docker pull google/cloud-sdk) > /dev/null;docker run -ti --rm --net="host" -v $HOME/.config:/root/.config -v /var/run/docker.sock:/var/run/docker.sock google/cloud-sdk gcloud"
alias gsutil="(docker images google/cloud-sdk || docker pull google/cloud-sdk) > /dev/null;docker run -ti --rm --net="host" -v $HOME/.config:/root/.config google/cloud-sdk gsutil"
