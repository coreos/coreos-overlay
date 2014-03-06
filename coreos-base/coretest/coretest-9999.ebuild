# Copyright (c) 2014 The CoreOS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coretest"
CROS_WORKON_LOCALNAME="coretest"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64 ~arm ~x86"
else
	CROS_WORKON_COMMIT="03512eea351cd7903b070b4969c1dd764a27928b"
	KEYWORDS="amd64 arm x86"
fi

inherit cros-workon

DESCRIPTION="Sanity tests for CoreOS"
HOMEPAGE="https://github.com/coreos/coretest"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.1"

src_compile() {
	./build || die
}

src_install() {
	dobin "${S}/${PN}"
}
