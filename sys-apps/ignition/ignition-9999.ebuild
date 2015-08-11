# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/ignition"
CROS_WORKON_LOCALNAME="ignition"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/ignition"
inherit coreos-doc coreos-go cros-workon systemd udev

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="41f6093552a80e8b78b769f9bca8599c8b269097" # tag v0.1.3
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="Pre-boot provisioning utility"
HOMEPAGE="https://github.com/coreos/ignition"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	GO_LDFLAGS="-X main.version $(git describe --dirty)" || die
	go_build "${COREOS_GO_PACKAGE}/src"
}

src_install() {
	newbin ${GOBIN}/src ${PN}

	udev_dorules "${FILESDIR}"/90-ignition.rules

	systemd_dounit "${FILESDIR}"/mnt-oem.mount
	systemd_dounit "${FILESDIR}"/ignition.target
	systemd_dounit "${FILESDIR}"/ignition-disks.service
	systemd_dounit "${FILESDIR}"/ignition-files.service

	coreos-dodoc -r doc/*
}
