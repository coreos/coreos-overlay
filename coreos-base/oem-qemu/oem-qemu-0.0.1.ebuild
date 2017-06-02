# Copyright (c) 2016 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=6

DESCRIPTION="OEM suite for QEMU"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE=""

# no source directory
S="${WORKDIR}"

src_install() {
	insinto "/usr/share/oem"
	doins "${FILESDIR}/grub.cfg"
}
