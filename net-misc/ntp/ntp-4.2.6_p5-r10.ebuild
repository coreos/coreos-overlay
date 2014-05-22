# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/ntp/ntp-4.2.6_p5-r10.ebuild,v 1.12 2014/04/06 15:03:40 vapier Exp $

EAPI="4"

inherit eutils toolchain-funcs flag-o-matic user systemd

MY_P=${P/_p/p}
DESCRIPTION="Network Time Protocol suite/programs"
HOMEPAGE="http://www.ntp.org/"
SRC_URI="http://www.eecis.udel.edu/~ntp/ntp_spool/ntp4/ntp-${PV:0:3}/${MY_P}.tar.gz
	mirror://gentoo/${MY_P}-manpages.tar.bz2"

LICENSE="HPND BSD ISC"
SLOT="0"
KEYWORDS="alpha amd64 arm arm64 hppa ia64 ~mips ppc ppc64 s390 sh sparc x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd ~x86-freebsd ~amd64-linux ~ia64-linux ~x86-linux ~m68k-mint"
IUSE="caps debug doc examples ipv6 openntpd parse-clocks samba selinux snmp ssl vim-syntax zeroconf"

DEPEND=">=sys-libs/ncurses-5.2
	>=sys-libs/readline-4.1
	kernel_linux? ( caps? ( sys-libs/libcap ) )
	zeroconf? ( net-dns/avahi[mdnsresponder-compat] )
	!openntpd? ( !net-misc/openntpd )
	snmp? ( net-analyzer/net-snmp )
	ssl? ( dev-libs/openssl )
	selinux? ( sec-policy/selinux-ntp )
	parse-clocks? ( net-misc/pps-tools )"
RDEPEND="${DEPEND}
	vim-syntax? ( app-vim/ntp-syntax )"
PDEPEND="openntpd? ( net-misc/openntpd )"

S=${WORKDIR}/${MY_P}

pkg_setup() {
	enewgroup ntp 123
	enewuser ntp 123 -1 /dev/null ntp
}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-4.2.4_p5-adjtimex.patch #254030
	epatch "${FILESDIR}"/${PN}-4.2.4_p7-nano.patch #270483
	append-cppflags -D_GNU_SOURCE #264109
}

src_configure() {
	# avoid libmd5/libelf
	export ac_cv_search_MD5Init=no ac_cv_header_md5_h=no
	export ac_cv_lib_elf_nlist=no
	# blah, no real configure options #176333
	export ac_cv_header_dns_sd_h=$(usex zeroconf)
	export ac_cv_lib_dns_sd_DNSServiceRegister=${ac_cv_header_dns_sd_h}
	econf \
		--with-lineeditlibs=readline,edit,editline \
		$(use_enable caps linuxcaps) \
		$(use_enable parse-clocks) \
		$(use_enable ipv6) \
		$(use_enable debug debugging) \
		$(use_enable samba ntp-signd) \
		$(use_with snmp ntpsnmpd) \
		$(use_with ssl crypto)
}

src_install() {
	default
	# move ntpd/ntpdate to sbin #66671
	dodir /usr/sbin
	mv "${ED}"/usr/bin/{ntpd,ntpdate} "${ED}"/usr/sbin/ || die "move to sbin"

	dodoc INSTALL WHERE-TO-START
	doman "${WORKDIR}"/man/*.[58]
	use doc && dohtml -r html/*

	insinto /usr/share/ntp
	doins "${FILESDIR}"/ntp.conf
	if use examples; then
		cp -r scripts/* "${ED}"/usr/share/ntp/ || die
		use prefix || fperms -R go-w /usr/share/ntp
		find "${ED}"/usr/share/ntp \
			'(' \
			-name '*.in' -o \
			-name 'Makefile*' -o \
			-name support \
			')' \
			-exec rm -r {} \;
	fi

	keepdir /var/lib/ntp
	use prefix || fowners ntp:ntp /var/lib/ntp
	systemd_newtmpfilesd "${FILESDIR}"/ntp.tmpfiles ntp.conf

	if use openntpd ; then
		cd "${ED}"
		rm usr/sbin/ntpd || die
		rm -r var/lib
		rm usr/share/man/*/ntpd.8 || die
	else
		systemd_dounit "${FILESDIR}"/ntpd.service
		systemd_enable_ntpunit 60-ntpd ntpd.service
		systemd_enable_service multi-user.target ntpd.service
		if ! use caps ; then
			sed -i "s|-u ntp:ntp||" \
				"${ED}/$(systemd_get_unitdir)/ntpd.service" || die
		fi
	fi

	systemd_dounit "${FILESDIR}"/ntpdate.service
	systemd_dounit "${FILESDIR}"/sntp.service
}
