# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=2
inherit flag-o-matic libtool

DESCRIPTION="Multilingual Library for Unix/Linux"
HOMEPAGE="http://www.m17n.org/m17n-lib/"
SRC_URI="mirror://gentoo/${P}.tar.gz"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 ppc ppc64 sh sparc x86"
IUSE=""

RDEPEND=">=dev-db/m17n-db-${PV}
	dev-libs/libxml2"

DEPEND="${RDEPEND}
	dev-util/pkgconfig"

src_prepare() {
	epatch "${FILESDIR}"/Fix-candidates-list-update-problem.path
	elibtoolize
}

src_configure() {
	append-flags -fPIC

	# The configure script warns that the --without-gui option is an
	# unrecognized option, but it's not true. Actually, the "unrecognized"
	# option effectively removes almost all optional modules of m17n-lib
	# that we don't use (namely X11, Xaw, fribidi, freetype, xft2, and
	# fontconfig modules). And as far as I know, there is no way to disable
	# these modules except the --without-gui option. Note that the X11 and
	# freetype modules in m17n-lib-1.6.1 does not compile on Chrome OS
	# since they are not cross-compile friendly.
	econf --without-gui || die
}

src_compile() {
	emake -j1 || die
}

src_install() {
	emake DESTDIR="${D}" install || die

	dodoc AUTHORS ChangeLog NEWS README TODO || die
}
