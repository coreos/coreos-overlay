# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-util/bsdiff/bsdiff-4.3-r2.ebuild,v 1.1 2010/12/13 00:35:03 flameeyes Exp $

EAPI=2

inherit eutils toolchain-funcs flag-o-matic

IUSE=""

DESCRIPTION="bsdiff: Binary Differencer using a suffix alg"
HOMEPAGE="http://www.daemonology.net/bsdiff/"
SRC_URI="http://www.daemonology.net/bsdiff/${P}.tar.gz"

SLOT="0"
LICENSE="BSD-2"
KEYWORDS="ppc64 alpha amd64 arm arm64 hppa ia64 mips ppc sparc x86 ~x86-fbsd ~x86-freebsd ~amd64-linux ~x86-linux ~ppc-macos"

RDEPEND="app-arch/bzip2"
DEPEND="${RDEPEND}"

src_prepare() {
	epatch ${FILESDIR}/4.3_bspatch-support-input-output-positioning.patch || die
	epatch ${FILESDIR}/4.3_bsdiff-convert-to-sais-lite-suffix-sort.patch || die
}

doecho() {
	echo "$@"
	"$@"
}

src_compile() {
	append-lfs-flags
	doecho $(tc-getCC) ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o bsdiff bsdiff.c sais.c -lbz2 || die "failed compiling bsdiff"
	doecho $(tc-getCC) ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} -o bspatch bspatch.c -lbz2 || die "failed compiling bspatch"
}

src_install() {
	dobin bs{diff,patch} || die
	doman bs{diff,patch}.1 || die
}
