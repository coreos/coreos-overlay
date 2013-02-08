# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-firewall/iptables/iptables-1.4.8-r1.ebuild,v 1.2 2010/05/25 13:20:57 pva Exp $

EAPI="2"
inherit eutils toolchain-funcs autotools

DESCRIPTION="Linux kernel (2.4+) firewall, NAT and packet mangling tools"
HOMEPAGE="http://www.iptables.org/"
SRC_URI="http://iptables.org/projects/iptables/files/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86"
IUSE="ipv6"

DEPEND="virtual/os-headers"
RDEPEND=""

src_prepare() {
	# use the saner headers from the kernel
	rm -f include/linux/{kernel,types}.h

	epatch "${FILESDIR}/${P}-build.patch" #321271
	epatch_user
	eautoreconf
}

src_configure() {
	econf \
		--sbindir=/sbin \
		--libexecdir=/$(get_libdir) \
		--enable-devel \
		--enable-libipq \
		--enable-shared \
		--enable-static \
		$(use_enable ipv6)
}

src_compile() {
	emake V=1 || die
}

src_install() {
	emake install DESTDIR="${D}" || die
	dosbin iptables-apply || die
	doman iptables-apply.8 || die
	dodoc INCOMPATIBILITIES iptables.xslt || die

	insinto /usr/include
	doins include/iptables.h $(use ipv6 && echo include/ip6tables.h) || die
	insinto /usr/include/iptables
	doins include/iptables/internal.h || die

	keepdir /var/lib/iptables
	newinitd "${FILESDIR}"/${PN}-1.3.2.init iptables || die
	newconfd "${FILESDIR}"/${PN}-1.3.2.confd iptables || die
	if use ipv6 ; then
		keepdir /var/lib/ip6tables
		newinitd "${FILESDIR}"/iptables-1.3.2.init ip6tables || die
		newconfd "${FILESDIR}"/ip6tables-1.3.2.confd ip6tables || die
	fi
}
