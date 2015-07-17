# Copyright (c) 2015 CoreOS, Inc.. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=5
CROS_WORKON_PROJECT="coreos/coreos-metadata"
CROS_WORKON_LOCALNAME="coreos-metadata"
CROS_WORKON_REPO="git://github.com"
inherit cros-workon systemd

if [[ "${PV}" == 9999 ]]; then
	KEYWORDS="~amd64"
else
	CROS_WORKON_COMMIT=""
	KEYWORDS="amd64"
fi

DESCRIPTION="coreos-metadata"
HOMEPAGE="https://github.com/coreos/coreos-metadata"
SRC_URI=""

LICENSE="Apache-2.0"
SLOT="0"
IUSE=""

DEPEND="
	dev-lang/rust-bin
	dev-rust/cargo
"

src_compile() {
	CARGO_HOME="${WORKDIR}/.cargo" cargo build \
		--release --verbose --no-default-features || die
}

src_install() {
	dobin "${S}/target/release/${PN}"
	systemd_dounit "${FILESDIR}/coreos-metadata.service"
}
