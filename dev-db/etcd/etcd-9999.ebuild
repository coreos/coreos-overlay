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
    CROS_WORKON_COMMIT=${CROS_WORKON_COMMIT:="HEAD"}
    KEYWORDS="~amd64 ~arm64"
elif [[ "${ARCH}" == "arm64" ]]; then
    # etcd-2.2.4 build fails for arm64
    # https://github.com/coreos/etcd/issues/3623
    CROS_WORKON_COMMIT="4669b899cca9a326731a10e03c76605bf26470f6"
    KEYWORDS="arm64"
else
    CROS_WORKON_COMMIT="bc9ddf260115d2680191c46977ae72b837785472" # v2.2.5
    KEYWORDS="amd64 arm64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="2"
IUSE=""

RDEPEND="!dev-db/etcd:0
	!dev-db/etcdctl"

src_compile() {
	GO_LDFLAGS="-X ${COREOS_GO_PACKAGE}/version.GitSHA ${CROS_WORKON_COMMIT:0:7}"
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
