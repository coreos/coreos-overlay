# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/iputils/iputils-20100418-r1.ebuild,v 1.1 2010/09/14 01:58:24 vapier Exp $

inherit flag-o-matic eutils toolchain-funcs

DESCRIPTION="Network monitoring tools including ping and ping6"
HOMEPAGE="http://www.linux-foundation.org/en/Net:Iputils"
SRC_URI="http://www.skbuff.net/iputils/iputils-s${PV}.tar.bz2
	mirror://gentoo/iputils-s${PV}-manpages.tar.bz2"

LICENSE="BSD"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~ppc-aix ~amd64-linux ~x86-linux"
IUSE="doc extras idn ipv6 SECURITY_HAZARD ssl static"

RDEPEND="extras? ( !net-misc/rarpd )
	ssl? ( dev-libs/openssl )
	idn? ( net-dns/libidn )"
DEPEND="${RDEPEND}
	virtual/os-headers"

S=${WORKDIR}/${PN}-s${PV}

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/021109-uclibc-no-ether_ntohost.patch
	epatch "${FILESDIR}"/${PN}-20100418-arping-broadcast.patch #337049
	epatch "${FILESDIR}"/${PN}-20100418-openssl.patch #335436
	epatch "${FILESDIR}"/${PN}-20100418-so_mark.patch #335347
	epatch "${FILESDIR}"/${PN}-20100418-makefile.patch
	epatch "${FILESDIR}"/${PN}-20100418-proper-libs.patch #332703
	epatch "${FILESDIR}"/${PN}-20100418-printf-size.patch
	epatch "${FILESDIR}"/${PN}-20100418-aliasing.patch
	epatch "${FILESDIR}"/${PN}-20071127-kernel-ifaddr.patch
	epatch "${FILESDIR}"/${PN}-20070202-idn.patch #218638
	epatch "${FILESDIR}"/${PN}-20100418-ping-CVE-2010-2529.patch #332527
	epatch "${FILESDIR}"/${PN}-20071127-infiniband.patch #377687
	use SECURITY_HAZARD && epatch "${FILESDIR}"/${PN}-20071127-nonroot-floodping.patch
	use static && append-ldflags -static
	use ssl && append-cppflags -DHAVE_OPENSSL
	use ipv6 || sed -i -e 's:IPV6_TARGETS=:#IPV6_TARGETS=:' Makefile
	export IDN=$(use idn && echo yes)
}

src_compile() {
	tc-export CC
	emake || die "make main failed"
}

src_install() {
	into /
	dobin ping || die "ping"
	use ipv6 && dobin ping6
	dosbin arping || die "arping"
	into /usr
	dosbin tracepath || die "tracepath"
	use ipv6 && dosbin trace{path,route}6
	use extras && \
		{ dosbin clockdiff rarpd rdisc ipg tftpd || die "misc sbin"; }

	fperms 4711 /bin/ping
	use ipv6 && fperms 4711 /bin/ping6 /usr/sbin/traceroute6

	dodoc INSTALL RELNOTES
	use ipv6 \
		&& dosym ping.8 /usr/share/man/man8/ping6.8 \
		|| rm -f doc/*6.8
	rm -f doc/setkey.8
	doman doc/*.8

	use doc && dohtml doc/*.html
}
