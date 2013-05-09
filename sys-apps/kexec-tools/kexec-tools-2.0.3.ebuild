# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/kexec-tools/kexec-tools-2.0.3.ebuild,v 1.1 2012/02/16 22:13:00 jlec Exp $

EAPI=4

inherit eutils flag-o-matic

DESCRIPTION="Load another kernel from the currently executing Linux kernel"
HOMEPAGE="http://kernel.org/pub/linux/utils/kernel/kexec/"
SRC_URI="mirror://kernel/linux/utils/kernel/kexec/${P}.tar.xz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="booke lzma xen zlib"

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
}

src_configure() {
	# GNU Make's $(COMPILE.S) passes ASFLAGS to $(CCAS), CCAS=$(CC)
	export ASFLAGS="${CCASFLAGS}"
	econf $(use_with lzma) $(use_with xen) $(use_with zlib) $(use_with booke)
}

src_install() {
	default

	newinitd "${FILESDIR}"/kexec.init-ng kexec
	newconfd "${FILESDIR}"/kexec.conf kexec
}
