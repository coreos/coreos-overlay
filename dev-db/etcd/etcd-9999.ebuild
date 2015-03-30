#
# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=5
CROS_WORKON_PROJECT="coreos/etcd"
CROS_WORKON_LOCALNAME="etcd"
CROS_WORKON_REPO="git://github.com"
COREOS_GO_PACKAGE="github.com/coreos/etcd"
inherit coreos-doc coreos-go toolchain-funcs cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="86e616c6e974828fc9119c1eb0f6439577a9ce0b" # v2.0.8
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="2"
IUSE=""

DEPEND=">=dev-lang/go-1.2"
RDEPEND="!dev-db/etcd:0
	!dev-db/etcdctl"

src_compile() {
	go_build "${COREOS_GO_PACKAGE}"
	go_build "${COREOS_GO_PACKAGE}/etcdctl"
}

src_install() {
	dobin ${WORKDIR}/gopath/bin/etcdctl
	newbin "${WORKDIR}/gopath/bin/${PN}" "${PN}${SLOT}"

	systemd_dounit "${FILESDIR}/${PN}${SLOT}.service"
	systemd_dotmpfilesd "${FILESDIR}/${PN}${SLOT}.conf"

	coreos-dodoc -r Documentation/*
}
