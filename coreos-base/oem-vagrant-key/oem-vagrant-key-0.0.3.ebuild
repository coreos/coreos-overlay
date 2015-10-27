# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for login access via Vagrant's ssh key."
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

# no source directory
S="${WORKDIR}"

src_install() {
	insinto "/usr/share/oem"
	doins "${FILESDIR}/cloud-config.yml"
	doins "${FILESDIR}/grub.cfg"
}
