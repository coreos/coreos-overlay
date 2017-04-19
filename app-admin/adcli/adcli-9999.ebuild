# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

if [[ ${PV} == *9999 ]]; then
	EGIT_REPO_URI="https://anongit.freedesktop.org/git/realmd/adcli.git"
	KEYWORDS="~amd64 ~arm64"
	inherit git-r3
else
	SRC_URI="https://www.freedesktop.org/software/realmd/releases/${P}.tar.gz"
	KEYWORDS="amd64 arm64"
fi

inherit autotools

DESCRIPTION="A helper library and tools for Active Directory client operations"
HOMEPAGE="https://www.freedesktop.org/software/realmd/adcli/"

LICENSE="LGPL-2.1+"
SLOT="0"
IUSE="debug doc"

RDEPEND="
	app-crypt/mit-krb5
	dev-libs/cyrus-sasl
	net-nds/openldap
"
DEPEND="${RDEPEND}
	doc? (
		app-text/docbook-xml-dtd:4.3
		dev-libs/libxslt
	)
"

src_prepare() {
	eapply_user
	eautoreconf
}

src_configure() {
	econf \
		$(use_enable debug) \
		$(use_enable doc)
}
