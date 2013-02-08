# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

inherit eutils

DESCRIPTION="X11 proxy that logs communication between a client and a server"
HOMEPAGE="http://xtrace.alioth.debian.org/"
SRC_URI="http://alioth.debian.org/frs/download.php/3201/${MY_PN}_${PV}.orig.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~sparc x86 ~x86-fbsd"
IUSE=""

RDEPEND="x11-libs/libX11"
DEPEND="${RDEPEND}"

src_prepare() {
	epatch "${FILESDIR}"/${PV}-print-uptime.patch
	epatch "${FILESDIR}"/${PV}-sync-extension.patch
}

src_install() {
	emake DESTDIR="${D}" install || die
}
