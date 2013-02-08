# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/keyutils/keyutils-1.1.ebuild,v 1.2 2006/11/26 10:25:32 peper Exp $

inherit eutils multilib

DESCRIPTION="Linux Key Management Utilities"
HOMEPAGE="http://www.kernel.org/"
SRC_URI="http://people.redhat.com/~dhowells/${PN}/${P}.tar.bz2"
LICENSE="GPL-2 LGPL-2.1"
SLOT="0"
KEYWORDS="~amd64 ~ppc ~x86"
IUSE=""
DEPEND=">=sys-kernel/linux-headers-2.6.11"
#RDEPEND=""

src_unpack() {
        unpack ${A}
        cd "${S}"
        epatch "${FILESDIR}/${P}-installdir.patch"
}

src_compile() {
	tc-getCC
	emake CFLAGS="-Wall ${CFLAGS}" NO_ARLIB=0
}

src_install() {
	emake install DESTDIR="${D}" LIBDIR="/usr/$(get_libdir)" USRLIBDIR="/usr/$(get_libdir)" NO_ARLIB=0
	dodoc README
}
