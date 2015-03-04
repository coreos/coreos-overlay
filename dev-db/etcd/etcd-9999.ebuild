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
    CROS_WORKON_COMMIT="1a2c6d3f2fbcb047712bfa3234bff5b9d07bdc57" # v2.0.4
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
	go_build "${COREOS_GO_PACKAGE}"
	go_build "${COREOS_GO_PACKAGE}/etcdctl"
	go_build "${COREOS_GO_PACKAGE}/tools/etcd-migrate"
	go_build "${COREOS_GO_PACKAGE}/tools/etcd-dump-logs"
}

src_install() {
	dobin ${WORKDIR}/gopath/bin/etcdctl
	dobin ${WORKDIR}/gopath/bin/etcd-migrate
	dobin ${WORKDIR}/gopath/bin/etcd-dump-logs

	exeinto /usr/libexec/${PN}/internal_versions
	newexe ${WORKDIR}/gopath/bin/${PN} ${ETCD_INTERNAL_VERSION}

	dosym ../libexec/${PN}/internal_versions/${ETCD_INTERNAL_VERSION} /usr/bin/${PN}

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_dotmpfilesd "${FILESDIR}"/${PN}.conf

	coreos-dodoc -r Documentation/*
}
