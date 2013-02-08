# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-db/m17n-db/m17n-db-1.5.5.ebuild,v 1.1 2010/01/26 15:26:00 matsuu Exp $

EAPI="2"
inherit eutils

DESCRIPTION="Database for the m17n library"
HOMEPAGE="http://www.m17n.org/m17n-lib/"
SRC_URI="mirror://gentoo/${P}.tar.gz"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 ppc ppc64 sh sparc x86"
IUSE=""

DEPEND="sys-devel/gettext"
RDEPEND="virtual/libintl"

src_prepare() {
	epatch "${FILESDIR}/do-not-commit-extra-space.patch"
}

src_install() {
	emake DESTDIR="${D}" install || die
	rm -rf "${D}/usr/share/m17n/icons" || die
	dodoc AUTHORS ChangeLog NEWS README || die
	docinto FORMATS; dodoc FORMATS/* || die
	docinto UNIDATA; dodoc UNIDATA/* || die
}
