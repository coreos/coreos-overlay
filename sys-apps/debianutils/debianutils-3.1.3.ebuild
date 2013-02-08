# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/debianutils/debianutils-3.1.3.ebuild,v 1.7 2009/07/13 18:22:10 josejx Exp $

inherit eutils flag-o-matic

DESCRIPTION="A selection of tools from Debian"
HOMEPAGE="http://packages.qa.debian.org/d/debianutils.html"
SRC_URI="mirror://debian/pool/main/d/${PN}/${PN}_${PV}.tar.gz"

LICENSE="BSD GPL-2 SMAIL"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc ~sparc-fbsd x86 ~x86-fbsd"
IUSE="kernel_linux static"

PDEPEND="|| ( >=sys-apps/coreutils-6.10-r1 sys-apps/mktemp sys-freebsd/freebsd-ubin )"

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${PN}-2.31-no-bs-namespace.patch
	epatch "${FILESDIR}"/${P}-installkernel-symlinks.patch
}

src_compile() {
	use static && append-ldflags -static
	econf || die
	emake || die
}

src_install() {
	into /
	dobin tempfile run-parts || die
	if use kernel_linux ; then
		dosbin installkernel || die "installkernel failed"
	fi

	into /usr
	dosbin savelog || die "savelog failed"

	doman tempfile.1 run-parts.8 savelog.8
	use kernel_linux && doman installkernel.8
	cd debian
	dodoc changelog control
}
