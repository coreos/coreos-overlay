# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/kexec-tools/kexec-tools-2.0.2-r1.ebuild,v 1.4 2011/09/21 15:28:08 chainsaw Exp $

EAPI=2

inherit eutils flag-o-matic

DESCRIPTION="Load another kernel from the currently executing Linux kernel"
HOMEPAGE="http://kernel.org/pub/linux/utils/kernel/kexec/"
SRC_URI="mirror://kernel/linux/utils/kernel/kexec/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE="lzma xen zlib"

DEPEND="
	lzma? ( app-arch/xz-utils )
	zlib? ( sys-libs/zlib )"
RDEPEND="${DEPEND}"

src_prepare() {
	epatch "${FILESDIR}/${PN}-2.0.0-respect-LDFLAGS.patch"

	# to disable the -fPIE -pie in the hardened compiler
	if gcc-specs-pie ; then
		filter-flags -fPIE
		append-ldflags -nopie
	fi

	# gcc 4.6 compatibility (bug #361069)
	sed -i 's/--no-undefined/-Wl,--no-undefined/g' purgatory/Makefile || die "sed failed"
}

src_configure() {
	# GNU Make's $(COMPILE.S) passes ASFLAGS to $(CCAS), CCAS=$(CC)
	export ASFLAGS="${CCASFLAGS}"
	econf $(use_with lzma) $(use_with xen) $(use_with zlib)
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"

	doman kexec/kexec.8 || die "doman failed"
	dodoc News AUTHORS TODO || die "dodoc failed"

	newinitd "${FILESDIR}"/kexec.init-ng kexec || die
	newconfd "${FILESDIR}"/kexec.conf kexec || die
}
