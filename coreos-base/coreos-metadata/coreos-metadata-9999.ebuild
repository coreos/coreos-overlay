# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/coreos-metadata"
inherit coreos-go cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
else
	CROS_WORKON_COMMIT="67305322c857e1e3f0c9b9f0924c9e60291981cd" # v0.2.2
	KEYWORDS="amd64 arm64"
fi

DESCRIPTION="A simple cloud-provider metadata agent"
HOMEPAGE="https://github.com/coreos/coreos-metadata"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

src_compile() {
	GO_LDFLAGS="-X main.version $(git describe --dirty)" || die
	go_build "${COREOS_GO_PACKAGE}/src"
}

src_install() {
	newbin "${GOBIN}/src" "${PN}"
	systemd_dounit "${FILESDIR}/coreos-metadata.service"
}
