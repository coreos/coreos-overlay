# Copyright 2013 The CoreOS Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/etcdctl"
CROS_WORKON_LOCALNAME="etcdctl"
CROS_WORKON_REPO="git://github.com"
CROS_WORKON_COMMIT="5aaeca137f947459c1b4588c5643a68a80592e03" # v0.1.2 tag + version flag

inherit cros-workon

DESCRIPTION="etcd command line client"
HOMEPAGE="https://github.com/coreos/etcdctl"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=">=dev-lang/go-1.1"
RDEPEND=""

src_compile() {
	./build
}

src_install() {
	dobin ${S}/${PN}
}
