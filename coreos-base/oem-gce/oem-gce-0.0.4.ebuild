# Copyright (c) 2013 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="OEM suite for Google Compute Engine images"
HOMEPAGE=""
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

# no source directory
S="${WORKDIR}"

RDEPEND="
	app-emulation/google-compute-daemon
	app-emulation/google-startup-scripts
"

src_prepare() {
	sed -e "s\\@@OEM_VERSION_ID@@\\${PVR}\\g" \
	    "${FILESDIR}/cloud-config.yml" > "${T}/cloud-config.yml" || die
}

src_install() {
	into "/usr/share/oem"
	dobin "${FILESDIR}/gce-ssh-key"
	dobin "${FILESDIR}/gce-coreos-cloudinit"
	dobin "${FILESDIR}/gce-add-metadata-host"
	dobin "${FILESDIR}/coreos-setup-environment"

	insinto "/usr/share/oem"
	doins "${T}/cloud-config.yml"
	doins "${FILESDIR}/grub.cfg"
}
