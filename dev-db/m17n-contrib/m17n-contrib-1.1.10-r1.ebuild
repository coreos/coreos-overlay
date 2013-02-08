# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

DESCRIPTION="Contribution database for the m17n library"
HOMEPAGE="http://www.m17n.org/m17n-lib/"
SRC_URI="mirror://gentoo/${P}.tar.gz"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND="dev-db/m17n-db"
RDEPEND="${DEPEND}"

src_configure() {
	# Since the HAVE_M17N_DB check in cofigure does not support cross
	# compilation, force to skip the check. The check is redundant --
	# we already have the DEPEND=m17n-db line above.
	export HAVE_M17N_DB=yes

	econf || die
}

src_install() {
	emake DESTDIR="${D}" install || die
	rm -rf "${D}/usr/share/m17n/icons" || die
	dodoc AUTHORS ChangeLog NEWS README || die
}
