# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/ignition"
CROS_WORKON_LOCALNAME="ignition"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/ignition"

KEYWORDS="~amd64 arm64"

inherit coreos-doc coreos-go cros-workon systemd

DESCRIPTION="Pre-boot provisioning utility"
HOMEPAGE="https://github.com/coreos/ignition"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/src"
}

src_install() {
	dobin ${GOBIN}/*

	systemd_dounit "${FILESDIR}"/coreos-metadata.target

	systemd_dounit "${FILESDIR}"/ignition@.service
	systemd_dounit "${FILESDIR}"/ignition-prepivot.target

	systemd_enable_service initrd.target ignition-prepivot.target

	coreos-dodoc -r doc/*
}
