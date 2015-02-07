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
    CROS_WORKON_COMMIT="199e2c43337dc18eafd7288ea65b5ff8944fccc4" # 0.3.1
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
	# TODO: this needs to be moved out once we can fetch an ACI for stage 1
	cros-workon_src_unpack
	${S}/stage1/rootfs/usr/cache.sh
	mv cache ${S}/stage1/rootfs/usr/
}

src_compile() {
	./build
}

src_install() {
	dobin ${S}/bin/rkt
	dobin ${S}/bin/stage1.aci
}
