# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/updatectl"
CROS_WORKON_LOCALNAME="updatectl"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="ba223e9b8c05ade82ee780b224d1707bfd2e0ce5"  # tag v1.1.0
	KEYWORDS="amd64"
fi

inherit cros-workon

DESCRIPTION="CoreUpdate Management CLI"
HOMEPAGE="https://github.com/coreos/updatectl"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	./build || die
}

src_install() {
	# Temporary until the name is fixed
	newbin ${S}/bin/${P} updatectl
}
