#!/bin/bash

set -euo pipefail

# Set up directory permissions/portage user and group.
sudo chmod a+rwX /etc/passwd /etc/group /etc /usr
echo "portage:x:250:250:portage:/var/tmp/portage:/bin/false" >> /etc/passwd
echo "portage::250:portage,travis" >> /etc/group

# Shuffle portage repositories around into the right places.
mkdir -p /etc/portage/repos.conf /usr/coreos-overlay
mv * /usr/coreos-overlay/
mv .git /usr/coreos-overlay/
git clone https://github.com/coreos/portage-stable /usr/portage/
cp .travis/coreos.conf /etc/portage/repos.conf/
ln -s /usr/coreos-overlay/profiles/coreos/amd64/sdk /etc/portage/make.profile
mkdir -p /usr/portage/metadata/{dtd,xml-schema}
wget -O /usr/portage/metadata/dtd/metadata.dtd https://www.gentoo.org/dtd/metadata.dtd
wget -O /usr/portage/metadata/xml-schema/metadata.xsd https://www.gentoo.org/xml-schema/metadata.xsd

# Download portage.
mkdir /tmp/portage && cd /tmp/portage
wget -qO - "https://gitweb.gentoo.org/proj/portage.git/snapshot/portage-${PORTAGE_VER}.tar.gz" | tar xz
