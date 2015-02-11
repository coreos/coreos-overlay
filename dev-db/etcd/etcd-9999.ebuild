#
# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/etcd"
CROS_WORKON_LOCALNAME="etcd"
CROS_WORKON_REPO="git://github.com"
inherit coreos-doc toolchain-funcs cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="fe1d9565c2619f7633d21d14c48588700edeed4d" # v2.0.1
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"
RDEPEND="!dev-db/etcdctl"

ETCD_INTERNAL_VERSION=2

src_compile() {
	./build
}

src_install() {
	exeinto /usr/libexec/${PN}/internal_versions
	newexe ${S}/bin/${PN} ${ETCD_INTERNAL_VERSION}

	dosym ../libexec/${PN}/internal_versions/${ETCD_INTERNAL_VERSION} /usr/bin/${PN}
	dobin ${S}/bin/etcdctl

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_dotmpfilesd "${FILESDIR}"/${PN}.conf

	coreos-dodoc -r Documentation/*
}
