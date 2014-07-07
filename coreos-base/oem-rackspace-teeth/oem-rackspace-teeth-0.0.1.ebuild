# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for Rackspace Teeth images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 x86"

# no source directory
S="${WORKDIR}"

src_install() {
	into "/usr/share/oem"
	dobin ${FILESDIR}/netname.sh

	insinto "/usr/share/oem"
	doins ${FILESDIR}/cloud-config.yml
}
