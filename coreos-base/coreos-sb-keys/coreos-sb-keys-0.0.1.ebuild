# Copyright (c) 2015 CoreOS Inc.
# Distributed under the terms of the GNU General Public License v2

EAPI=5

DESCRIPTION="CoreOS Secure Boot keys"
HOMEPAGE=""
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86 ppc64"
IUSE=""

S="${WORKDIR}"

src_install() {
	insinto /usr/share/sb_keys
	newins "${FILESDIR}/PK.key" PK.key
	newins "${FILESDIR}/PK.crt" PK.crt
	newins "${FILESDIR}/KEK.key" KEK.key
	newins "${FILESDIR}/KEK.crt" KEK.crt
	newins "${FILESDIR}/DB.key" DB.key
	newins "${FILESDIR}/DB.crt" DB.crt
}
