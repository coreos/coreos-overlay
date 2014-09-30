# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for VMware"
HOMEPAGE="https://github.com/coreos/coreos-overlay/tree/master/coreos-base"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="
	app-emulation/open-vm-tools
	>=sys-apps/systemd-215-r15
	"
RDEPEND="${DEPEND}"

# no source directory
S="${WORKDIR}"

src_prepare() {
	sed -e "s\\@@OEM_VERSION_ID@@\\${PVR}\\g" \
		"${FILESDIR}"/cloud-config.yml > ${T}/cloud-config.yml || die
}

src_install() {
	insinto "/usr/share/oem"
	doins ${T}/cloud-config.yml

	insinto "/usr/lib/systemd/network"
	doins "${FILESDIR}"/zz-default.network
}
