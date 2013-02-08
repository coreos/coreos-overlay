# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-cpp/gmock/gmock-1.5.0.ebuild,v 1.2 2011/11/11 20:12:12 vapier Exp $

EAPI="4"

inherit libtool cros-au

MY_PN=${PN%32}
MY_P="${MY_PN}-${PV}"

DESCRIPTION="Google's C++ mocking framework"
HOMEPAGE="http://code.google.com/p/googlemock/"
SRC_URI="http://googlemock.googlecode.com/files/${MY_P}.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="32bit_au"
REQUIRED_USE="32bit_au"
RESTRICT="test"

RDEPEND=">=dev-cpp/gtest32-${PV}"
DEPEND="${RDEPEND}"

S="${WORKDIR}/${MY_P}"

src_unpack() {
	default
	# make sure we always use the system one
	rm -r "${S}"/gtest/Makefile* || die
}

src_prepare() {
	elibtoolize
}

src_configure() {
	board_setup_32bit_au_env
	econf --disable-shared --enable-static
}

src_install() {
	dolib.a lib/.libs/libgmock{,_main}.a
}
