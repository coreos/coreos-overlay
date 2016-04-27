# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/ntp/ntp-4.2.8-r2.ebuild,v 1.2 2015/02/26 18:30:06 floppym Exp $

EAPI="4"

inherit autotools eutils toolchain-funcs flag-o-matic user systemd

MY_P=${P/_p/p}
DESCRIPTION="Network Time Protocol suite/programs"
HOMEPAGE="http://www.ntp.org/"
SRC_URI="http://www.eecis.udel.edu/~ntp/ntp_spool/ntp4/ntp-${PV:0:3}/${MY_P}.tar.gz"

LICENSE="HPND BSD ISC"
SLOT="0"
KEYWORDS="~alpha amd64 ~arm ~arm64 ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc ~x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd ~x86-freebsd ~amd64-linux ~ia64-linux ~x86-linux ~m68k-mint"
IUSE="caps debug ipv6 openntpd parse-clocks perl samba selinux snmp ssl threads vim-syntax zeroconf"

CDEPEND=">=sys-libs/ncurses-5.2
	>=sys-libs/readline-4.1
	>=dev-libs/libevent-2.0.9[threads?]
	kernel_linux? ( caps? ( sys-libs/libcap ) )
	zeroconf? ( net-dns/avahi[mdnsresponder-compat] )
	!openntpd? ( !net-misc/openntpd )
	snmp? ( net-analyzer/net-snmp )
	ssl? ( dev-libs/openssl )
	parse-clocks? ( net-misc/pps-tools )"
DEPEND="${CDEPEND}
	virtual/pkgconfig"
RDEPEND="${CDEPEND}
	selinux? ( sec-policy/selinux-ntp )
	vim-syntax? ( app-vim/ntp-syntax )"
PDEPEND="openntpd? ( net-misc/openntpd )"

S=${WORKDIR}/${MY_P}

pkg_setup() {
	enewgroup ntp 123
	enewuser ntp 123 -1 /dev/null ntp
}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-4.2.8-ipc-caps.patch #533966
	epatch "${FILESDIR}"/${PN}-4.2.8-sntp-test-pthreads.patch #563922
	epatch "${FILESDIR}"/${PN}-4.2.8-ntpd-test-signd.patch
	use perl || epatch "${FILESDIR}"/${PN}-4.2.8-disable-perl-scripts.patch
	append-cppflags -D_GNU_SOURCE #264109
	# Make sure every build uses the same install layout. #539092
	find sntp/loc/ -type f '!' -name legacy -delete || die
	# Disable pointless checks.
	touch .checkChangeLog .gcc-warning FRC.html html/.datecheck
	eautoreconf
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
		--with-yielding-select \
		--disable-local-libevent \
		--docdir="${EPREFIX}/usr/share/doc/${PF}" \
		--htmldir="${EPREFIX}/usr/share/doc/${PF}/html" \
		$(use_enable caps linuxcaps) \
		$(use_enable parse-clocks) \
		$(use_enable ipv6) \
		$(use_enable debug debugging) \
		$(use_enable samba ntp-signd) \
		$(use_with snmp ntpsnmpd) \
		$(use_with ssl crypto) \
		$(use_enable threads thread-support)
}

src_install() {
	default
	# move ntpd/ntpdate to sbin #66671
	dodir /usr/sbin
	mv "${ED}"/usr/bin/{ntpd,ntpdate} "${ED}"/usr/sbin/ || die "move to sbin"

	dodoc INSTALL WHERE-TO-START

	insinto /usr/share/ntp
	doins "${FILESDIR}"/ntp.conf
	systemd_newtmpfilesd "${FILESDIR}"/ntp.tmpfiles ntp.conf

	keepdir /var/lib/ntp
	use prefix || fowners ntp:ntp /var/lib/ntp

	if use openntpd ; then
		cd "${ED}"
		rm usr/sbin/ntpd || die
		rm -r var/lib
	else
		systemd_dounit "${FILESDIR}"/ntpd.service
		use caps && sed -i '/ExecStart/ s|$| -u ntp:ntp|' "${ED}"/usr/lib/systemd/system/ntpd.service
		systemd_enable_ntpunit 60-ntpd ntpd.service
	fi

	systemd_dounit "${FILESDIR}"/ntpdate.service
	systemd_dounit "${FILESDIR}"/sntp.service
}
