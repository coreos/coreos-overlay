# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/fleet"
CROS_WORKON_LOCALNAME="fleet"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/fleet"
inherit coreos-doc cros-workon coreos-go systemd

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="f0cbed22bcc50ca63ef6d09590592e7f65b04871"  # tag v0.11.4
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="fleet"
HOMEPAGE="https://github.com/coreos/fleet"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/fleetd"
	go_build "${COREOS_GO_PACKAGE}/fleetctl"
}

src_install() {
	dobin ${GOBIN}/*
	dosym ./fleetd /usr/bin/fleet

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_dounit "${FILESDIR}"/${PN}.socket

	coreos-dodoc -r Documentation/*
}
