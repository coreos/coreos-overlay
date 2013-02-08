# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-power/acpid/acpid-1.0.10_p3.ebuild,v 1.3 2009/05/07 16:20:16 armin76 Exp $

inherit toolchain-funcs

MY_P="${P%_p*}-netlink${PV#*_p}"
S="${WORKDIR}/${MY_P}"

DESCRIPTION="Daemon for Advanced Configuration and Power Interface"
HOMEPAGE="http://acpid.sourceforge.net"
SRC_URI="http://tedfelix.com/linux/${MY_P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 ia64 -ppc x86"
IUSE=""

DEPEND="sys-apps/sed"
RDEPEND=""

src_unpack() {
	unpack ${A}
	sed -i -e '/^CFLAGS /{s:=:+=:;s:-Werror::;s:-O2 -g::}' "${S}"/Makefile || die
}

src_compile() {
	emake CC="$(tc-getCC)" INSTPREFIX="${D}" || die "emake failed"
}

src_install() {
	emake INSTPREFIX="${D}" install || die "emake install failed"

	dodoc README Changelog TODO || die

	newinitd "${FILESDIR}"/${PN}-1.0.6-init.d acpid || die
	newconfd "${FILESDIR}"/${PN}-1.0.6-conf.d acpid || die
}

pkg_postinst() {
	echo
	einfo "You may wish to read the Gentoo Linux Power Management Guide,"
	einfo "which can be found online at:"
	einfo "    http://www.gentoo.org/doc/en/power-management-guide.xml"
	echo
}
