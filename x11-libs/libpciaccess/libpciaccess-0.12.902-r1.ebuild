# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-libs/libpciaccess/libpciaccess-0.12.902.ebuild,v 1.1 2011/12/19 01:39:15 chithanh Exp $

EAPI=4
inherit xorg-2

DESCRIPTION="Library providing generic access to the PCI bus and devices"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~sh ~sparc x86 ~x86-fbsd ~x64-freebsd ~x86-freebsd ~amd64-linux ~x86-linux ~sparc-solaris ~x64-solaris ~x86-solaris"
IUSE="minimal zlib"

DEPEND="!<x11-base/xorg-server-1.5
	zlib? ( sys-libs/zlib )"
RDEPEND="${DEPEND}"

PATCHES=( "${FILESDIR}/nodevport.patch" )

pkg_setup() {
	xorg-2_pkg_setup

	XORG_CONFIGURE_OPTIONS=(
		"$(use_with zlib)"
		"--with-pciids-path=${EPREFIX}/usr/share/misc"
	)
}

src_install() {
	xorg-2_src_install
	if ! use minimal; then
		dodir /usr/bin || die
		${BASH} "${AUTOTOOLS_BUILD_DIR:-${S}}/libtool" --mode=install "$(type -P install)" -c "${AUTOTOOLS_BUILD_DIR:-${S}}/scanpci/scanpci" "${ED}"/usr/bin || die
	fi
}
