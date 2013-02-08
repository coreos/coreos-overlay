# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/glib/glib-1.2.10-r5.ebuild,v 1.53 2008/08/03 22:36:31 eva Exp $

inherit autotools libtool flag-o-matic eutils portability

DESCRIPTION="The GLib library of C routines"
HOMEPAGE="http://www.gtk.org/"
SRC_URI="ftp://ftp.gtk.org/pub/gtk/v1.2/${P}.tar.gz
	 ftp://ftp.gnome.org/pub/GNOME/stable/sources/glib/${P}.tar.gz
	 mirror://gentoo/glib-1.2.10-r1-as-needed.patch.bz2"

LICENSE="LGPL-2.1"
SLOT="1"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~x86-fbsd"
IUSE="hardened"

DEPEND=""

src_unpack() {
	unpack ${A}
	cd "${S}"

	epatch "${FILESDIR}"/${P}-automake.patch
	epatch "${FILESDIR}"/${P}-m4.patch
	epatch "${FILESDIR}"/${P}-configure-LANG.patch #133679

	# Allow glib to build with gcc-3.4.x #47047
	epatch "${FILESDIR}"/${P}-gcc34-fix.patch

	# Fix for -Wl,--as-needed (bug #133818)
	epatch "${DISTDIR}"/glib-1.2.10-r1-as-needed.patch.bz2

	use ppc64 && use hardened && replace-flags -O[2-3] -O1
	append-ldflags $(dlopen_lib)

	rm -f acinclude.m4 #168198
	eautoreconf
	elibtoolize
}

src_compile() {
	# Bug 48839: pam fails to build on ia64
	# The problem is that it attempts to link a shared object against
	# libglib.a; this library needs to be built with -fPIC.  Since
	# this package doesn't contain any significant binaries, build the
	# whole thing with -fPIC (23 Apr 2004 agriffis)
	append-flags -fPIC

	econf \
		--with-threads=posix \
		--enable-debug=yes \
		|| die
	emake || die
}

src_install() {
	make install DESTDIR="${D}" || die

	dodoc AUTHORS ChangeLog README* INSTALL NEWS
	dohtml -r docs

	cd "${D}"/usr/$(get_libdir) || die
	chmod 755 libgmodule-1.2.so.*
}
