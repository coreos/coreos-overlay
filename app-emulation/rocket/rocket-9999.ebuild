#
# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=4
CROS_WORKON_PROJECT="coreos/rocket"
CROS_WORKON_LOCALNAME="rocket"
CROS_WORKON_REPO="git://github.com"
inherit cros-workon

if [[ "${PV}" == 9999 ]]; then
    KEYWORDS="~amd64"
else
    CROS_WORKON_COMMIT="c94b5338411a3dbfb0d9cf62e9066fb837c1baae" # v0.3.2
    KEYWORDS="amd64"
fi

DESCRIPTION="rocket"
HOMEPAGE="https://github.com/coreos/rocket"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND="
  >=dev-lang/go-1.2
  dev-util/go-bindata
"

src_unpack() {
	cros-workon_src_unpack
	${S}/stage1/rootfs/usr/cache.sh
	mv cache ${S}/stage1/rootfs/usr/
	GOPATH=${S}/gopath go get github.com/appc/spec/...
}

src_compile() {
	./build
}

src_install() {
	dobin ${S}/bin/rkt
}
