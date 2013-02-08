# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-misc/unclutter/unclutter-8-r1.ebuild,v 1.9 2007/07/22 03:41:55 dberkholz Exp $

inherit eutils toolchain-funcs

S="${WORKDIR}/${PN}"
DESCRIPTION="Hides mouse pointer while not in use."
HOMEPAGE="http://www.ibiblio.org/pub/X11/contrib/utilities/unclutter-8.README"
SRC_URI="ftp://ftp.x.org/contrib/utilities/${P}.tar.Z"
SLOT="0"
LICENSE="public-domain"
KEYWORDS="alpha amd64 arm hppa ~mips ppc ppc64 ~sparc x86"
IUSE=""
RDEPEND="x11-libs/libX11
	x11-libs/libXext
	x11-libs/libXi"
DEPEND="${RDEPEND}
	x11-proto/xproto"

src_compile() {
	# src_prepare fails in this ebuild, so patch here
	epatch "${FILESDIR}/${P}-keypress.patch"
	epatch "${FILESDIR}/${P}-listen-to-all-windows.patch"
	epatch "${FILESDIR}/${P}-ignore-some-keys.patch"
	epatch "${FILESDIR}/${P}-disable-tap.patch"
	# This xmkmf appears unnecessary
	# xmkmf -a || die "Couldn't run xmkmf"
	emake -j1 CDEBUGFLAGS="${CFLAGS}" CC="$(tc-getCC)" || die
}

src_install () {
	dobin unclutter
	newman unclutter.man unclutter.1x
	dodoc README
	insinto /etc/init
	doins "${FILESDIR}/unclutter.conf"
}
