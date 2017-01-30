# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI=5
EGO_PN="github.com/docker/libnetwork"

COREOS_GO_PACKAGE="${EGO_PN}"
COREOS_GO_VERSION="go1.7"

if [[ ${PV} == *9999 ]]; then
	KEYWORDS="~amd64 ~arm64"
	inherit golang-vcs
else
	EGIT_COMMIT="57be722e077059d1ee0539be31743a3642ccbeb3"
	SRC_URI="https://${EGO_PN}/archive/${EGIT_COMMIT}.tar.gz -> ${P}.tar.gz"
	KEYWORDS="amd64 arm64"
	inherit golang-vcs-snapshot
fi

inherit coreos-go

DESCRIPTION="Docker container networking"
HOMEPAGE="http://github.com/docker/libnetwork"

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

S=${WORKDIR}/${P}/src/${EGO_PN}

RDEPEND="!<app-emulation/docker-1.13.0_rc1"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/cmd/proxy"
}

src_install() {
	newbin "${GOBIN}"/proxy docker-proxy
}
