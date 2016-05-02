# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/ustr/ustr-1.0.4-r5.ebuild,v 1.2 2014/12/02 12:48:28 pacho Exp $

EAPI=5

inherit eutils autotools

DESCRIPTION="Low-overhead managed string library for C"
HOMEPAGE="http://www.and.org/ustr"
SRC_URI="ftp://ftp.and.org/pub/james/ustr/${PV}/${P}.tar.bz2"

LICENSE="|| ( BSD-2 MIT LGPL-2 )"
SLOT="0"
KEYWORDS="amd64 arm64 ppc64"
IUSE=""

DEPEND=""
RDEPEND=""

src_prepare() {
	# cross-compile patches from debian
	epatch "${FILESDIR}"/autoconf.diff
	epatch "${FILESDIR}"/gnu-inline.diff
	epatch "${FILESDIR}"/stdarg-va_copy.diff
	epatch "${FILESDIR}"/unused-vars.diff

	eautoreconf
}
