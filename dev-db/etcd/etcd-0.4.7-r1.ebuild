#
# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/etcd"
CROS_WORKON_LOCALNAME="etcd"
CROS_WORKON_REPO="git://github.com"
inherit toolchain-funcs cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="d6523fe4638100c72f40cb282cd1232db13f7336" # v0.4.7
    KEYWORDS="amd64"
fi

DESCRIPTION="etcd"
HOMEPAGE="https://github.com/coreos/etcd"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="protocol-1"
IUSE=""

DEPEND=">=dev-lang/go-1.2"
RDEPEND=">=dev-db/etcd-2.0.1"

ETCD_INTERNAL_VERSION=1

src_compile() {
	./build
}

src_install() {
	exeinto /usr/libexec/${PN}/internal_versions
	newexe ${S}/bin/${PN} ${ETCD_INTERNAL_VERSION}
}
