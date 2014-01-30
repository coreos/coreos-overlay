# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/coreinit"
CROS_WORKON_LOCALNAME="coreinit"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="02daf08f4ae548ad430d4e5084dc3b0de1138221"
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="coreinit"
HOMEPAGE="https://github.com/coreos/coreinit"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.1"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/bin/coreinit
	dobin ${S}/bin/corectl

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_enable_service multi-user.target ${PN}.service
}
