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
    CROS_WORKON_COMMIT="d74e74d320666044a554205ac6307349239d4b4f" # v2.0.0+
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="1"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

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
