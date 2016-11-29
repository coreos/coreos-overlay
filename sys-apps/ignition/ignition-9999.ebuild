# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/ignition"
CROS_WORKON_LOCALNAME="ignition"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/ignition"
inherit coreos-go cros-workon systemd udev

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="9dabbe8d61b6d60587b6dadd8d1cb58d35b73ccb" # tag v0.12.0
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="Pre-boot provisioning utility"
HOMEPAGE="https://github.com/coreos/ignition"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0/${PVR}"
IUSE=""

src_compile() {
	export GO15VENDOREXPERIMENT="1"
	GO_LDFLAGS="-X github.com/coreos/ignition/internal/version.Raw=$(git describe --dirty)" || die
	go_build "${COREOS_GO_PACKAGE}/internal"
}

src_install() {
	newbin ${GOBIN}/internal ${PN}

	systemd_dounit "${FILESDIR}"/ignition.target
	systemd_dounit "${FILESDIR}"/ignition-disks.service
	systemd_dounit "${FILESDIR}"/ignition-files.service
}
