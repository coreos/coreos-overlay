# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/etcdctl"
CROS_WORKON_LOCALNAME="etcdctl"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="061135b2a02797a6b3c2b6c01183517c1bc76a2c" # v0.4.3
    KEYWORDS="amd64"
fi

inherit cros-workon

DESCRIPTION="etcd command line client"
HOMEPAGE="https://github.com/coreos/etcdctl"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"
RDEPEND=""

src_compile() {
	./build
}

src_install() {
	dobin ${S}/bin/${PN}
}
