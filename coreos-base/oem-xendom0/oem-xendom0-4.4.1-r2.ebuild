# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for Xen testing"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

# no source directory
S="${WORKDIR}"

src_prepare() {
	sed -e "s%@@OEM_VERSION_ID@@%${PVR}%g" \
	    "${FILESDIR}/cloud-config.yml" > "${T}/cloud-config.yml" || die
	sed -e "s%@@XEN_VERSION@@%${PV}%g" \
	    "${FILESDIR}/grub.cfg" > "${T}/grub.cfg" || die
}

src_install() {
	insinto "/usr/share/oem"
	doins "${T}/cloud-config.yml"
	doins "${T}/grub.cfg"
}
