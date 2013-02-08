# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-apps/xinit/xinit-1.3.0-r1.ebuild,v 1.3 2010/12/19 12:21:00 ssuominen Exp $

EAPI="4"

inherit toolchain-funcs

DESCRIPTION="X.Org xset application stripped down for just 'r' and 'm' commands"
HOMEPAGE="http://xorg.freedesktop.org/"
SRC_URI=""

LICENSE="MIT"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 mips ppc ppc64 s390 sh sparc x86 ~x86-fbsd"
IUSE="+minimal"

RDEPEND="x11-libs/libXmu
	x11-libs/libX11"
DEPEND="${RDEPEND}"

S=${WORKDIR}

src_unpack() {
	cp "${FILESDIR}"/* ./ || die
}

src_configure() {
	tc-export CC
}
