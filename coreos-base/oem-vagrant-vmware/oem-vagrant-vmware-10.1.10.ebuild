# Copyright 2017 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for vagrant images (vmware)"
HOMEPAGE="https://github.com/coreos/coreos-overlay/tree/master/coreos-base"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND="
	app-emulation/open-vm-tools
	"
RDEPEND="${DEPEND}"

# no source directory
S="${WORKDIR}"

src_install() {
	insinto "/usr/share/oem"
	doins -r "${FILESDIR}/box"
	doins "${FILESDIR}/grub.cfg"
	doins -r "${FILESDIR}/units"
}
