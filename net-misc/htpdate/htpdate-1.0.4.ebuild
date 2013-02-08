# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-misc/htpdate/htpdate-1.0.4.ebuild,v 1.7 2009/06/19 22:08:34 ranger Exp $

inherit toolchain-funcs eutils

DESCRIPTION="Synchronize local workstation with time offered by remote webservers"
HOMEPAGE="http://www.clevervest.com/htp/"
SRC_URI="http://www.clevervest.com/htp/archive/c/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 arm hppa ~mips ppc ~ppc64 s390 sh x86"
IUSE=""

DEPEND=""
RDEPEND=""


src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}/${P}-waitforvalidhttpresp.patch"
	epatch "${FILESDIR}/${P}-errorcheckhttpresp.patch"
	epatch "${FILESDIR}/${P}-checkagainstbuildtime.patch"
	epatch "${FILESDIR}/${P}-oob_date_read.patch"
	epatch "${FILESDIR}/${P}-errorsarentsuccess.patch"
	epatch "${FILESDIR}/${P}-64bit_limits.patch"
	epatch "${FILESDIR}/${P}-relative_path.patch"
	epatch "${FILESDIR}/${P}-all_headers.patch"
	gunzip htpdate.8.gz || die
}

src_compile() {
	# Provide timestamp of when this was built, in number of seconds since
	# 01 Jan 1970 in UTC time.
	DATE=`date -u +%s`
	# Set it back one day to avoid dealing with time zones.
	DATE_OPT="-DBUILD_TIME_UTC=`expr $DATE - 86400`"
	emake CFLAGS="-Wall $DATE_OPT ${CFLAGS} ${LDFLAGS}" CC="$(tc-getCC)" || die
}

src_install() {
	dosbin htpdate || die
	doman htpdate.8
	dodoc README Changelog

	newconfd "${FILESDIR}"/htpdate.conf htpdate
	newinitd "${FILESDIR}"/htpdate.init htpdate
}

pkg_postinst() {
	einfo "If you would like to run htpdate as a daemon set"
	einfo "appropriate http servers in /etc/conf.d/htpdate!"
}
