# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit autotools eutils

DESCRIPTION="Upstart is an event-based replacement for the init daemon"
HOMEPAGE="http://upstart.ubuntu.com/"
SRC_URI="http://upstart.ubuntu.com/download/0.6/${P}.tar.bz2"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE="examples nls"

DEPEND=">=dev-libs/expat-2.0.0
	>=sys-apps/dbus-1.2.16
	nls? ( sys-devel/gettext )"

RDEPEND=">=sys-apps/dbus-1.2.16"

src_unpack() {
	unpack $A
	cd "${S}"

	epatch "${FILESDIR}"/${P}-cross-compile.patch   # Already upstream
	epatch "${FILESDIR}"/${P}-cross-compile2.patch  # Not upstream @ 1214

	eautoreconf
}

src_compile() {
	econf --prefix=/ --includedir='${prefix}/usr/include' $(use_enable nls) || die "econf failed"

	emake NIH_DBUS_TOOL=$(which nih-dbus-tool) \
		|| die "emake failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "make install failed"

	if ! use examples ; then
		elog "Removing example .conf files."
		rm "${D}"/etc/init/*.conf
	fi
}
