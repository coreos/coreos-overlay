# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=5

EGO_PN="github.com/coreos/etcd"
COREOS_GO_PACKAGE="${EGO_PN}"
DESCRIPTION="The etcd command line client, v3, compatible with v2"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI="https://${EGO_PN}/archive/v${PV}.tar.gz -> ${P}.tar.gz"

inherit coreos-go golang-vcs-snapshot

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64 arm64"
IUSE=""

S=${WORKDIR}/${P}/src/${EGO_PN}

DEPEND=""
RDEPEND="${DEPEND}"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}/cmd/etcdctl"
}

src_install() {
	dobin ${WORKDIR}/gopath/bin/etcdctl
}
