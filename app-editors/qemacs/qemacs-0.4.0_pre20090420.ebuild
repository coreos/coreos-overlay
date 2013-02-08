# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-editors/qemacs/qemacs-0.4.0_pre20090420.ebuild,v 1.4 2009/12/08 19:33:49 nixnut Exp $

EAPI=2

inherit eutils flag-o-matic toolchain-funcs

DESCRIPTION="QEmacs is a very small but powerful UNIX editor"
HOMEPAGE="http://savannah.nongnu.org/projects/qemacs"
SRC_URI="mirror://gentoo/${P}.tar.bz2"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64 ppc x86"
IUSE="X png unicode xv"
RESTRICT="strip test"

RDEPEND="!app-editors/qe
	X? ( x11-libs/libX11
		x11-libs/libXext
		xv? ( x11-libs/libXv ) )
	png? ( media-libs/libpng:1.2 )"

DEPEND="${RDEPEND}
	app-text/texi2html"

S="${WORKDIR}/${PN}"

src_prepare() {
	# Removes forced march setting and align-functions on x86, as they
	# would override user's CFLAGS..
	epatch "${FILESDIR}/${PN}-0.4.0_pre20080605-Makefile.patch"
	# Make backup files optional
	epatch "${FILESDIR}/${PN}-0.4.0_pre20080605-make_backup.patch"
	# Suppress stripping
	epatch "${FILESDIR}/${P}-nostrip.patch"

	use unicode && epatch "${FILESDIR}/${PN}-0.3.2_pre20070226-tty_utf8.patch"

	# Change the manpage to reference a /real/ file instead of just an
	# approximation.  Purely cosmetic!
	sed -i "s,^/usr/share/doc/qe,&-${PVR}," qe.1 || die
}

src_configure() {
	# when using any other CFLAGS than -O0, qemacs will segfault on startup,
	# see bug 92011
	replace-flags -O? -O0
	econf --cross-prefix="${CHOST}-" \
		$(use_enable X x11) \
		$(use_enable png) \
		$(use_enable xv)
}

src_compile() {
	# Does not support parallel building
	emake -j1 || die
}

src_install() {
	emake install DESTDIR="${D}" || die
	dodoc Changelog README TODO config.eg || die
	dohtml *.html || die

	# Fix man page location
	mv "${D}"/usr/{,share/}man || die

	# Install headers so users can build their own plugins.
	insinto /usr/include/qe
	doins cfb.h config.h cutils.h display.h fbfrender.h libfbf.h qe.h \
		qeconfig.h qestyles.h qfribidi.h || die
	cd libqhtml
	insinto /usr/include/qe/libqhtml
	doins css.h cssid.h htmlent.h || die
}
