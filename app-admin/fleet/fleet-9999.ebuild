# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/fleet"
CROS_WORKON_LOCALNAME="fleet"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="2d5c6bb0751ad3a50c0a219158439071a59e549f"  # tag v0.9.0
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="fleet"
HOMEPAGE="https://github.com/coreos/fleet"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.3"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/bin/fleetd
	dosym ./fleetd /usr/bin/fleet

	dobin ${S}/bin/fleetctl

	systemd_dounit "${FILESDIR}"/${PN}.service
	systemd_dounit "${FILESDIR}"/${PN}.socket
}
