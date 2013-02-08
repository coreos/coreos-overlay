# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-cpp/gtest/gtest-1.4.0.ebuild,v 1.1 2011/11/11 20:09:57 vapier Exp $

EAPI="4"
inherit autotools eutils cros-au

MY_PN=${PN%32}
MY_P="${MY_PN}-${PV}"

DESCRIPTION="Google C++ Testing Framework"
HOMEPAGE="http://code.google.com/p/googletest/"
SRC_URI="http://googletest.googlecode.com/files/${MY_P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="32bit_au"
REQUIRED_USE="32bit_au"
RESTRICT="test"

DEPEND="dev-lang/python"
RDEPEND=""

S="${WORKDIR}/${MY_P}"

src_prepare() {
	sed -i -e "s|/tmp|${T}|g" test/gtest-filepath_test.cc || die "sed failed"
	epatch "${FILESDIR}"/${MY_P}-asneeded.patch
	epatch "${FILESDIR}"/${MY_P}-gcc-4.7.patch
	eautoreconf
}

src_configure() {
	board_setup_32bit_au_env
	econf --disable-shared --enable-static
}

src_install() {
	dolib.a lib/.libs/libgtest{,_main}.a
}
