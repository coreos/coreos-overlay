# Copyright (c) 2014 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="coreos/fleet"
CROS_WORKON_LOCALNAME="fleet"
CROS_WORKON_REPO="git://github.com"

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT="dec212e44a3c306064b6d29381c6fb5b587121c7"  # tag v0.2.0
	KEYWORDS="amd64"
fi

inherit cros-workon systemd

DESCRIPTION="fleet"
HOMEPAGE="https://github.com/coreos/fleet"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND=">=dev-lang/go-1.2"

src_compile() {
	./build || die
}

src_install() {
	dobin ${S}/bin/fleet
	dobin ${S}/bin/fleetctl

	systemd_dounit "${FILESDIR}"/${PN}.service
}
