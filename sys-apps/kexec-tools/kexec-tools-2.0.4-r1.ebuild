# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/kexec-tools/kexec-tools-2.0.4-r1.ebuild,v 1.1 2013/03/30 13:01:49 jlec Exp $

EAPI=5

inherit eutils flag-o-matic linux-info

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

CONFIG_CHECK="~KEXEC"

src_prepare() {
	epatch \
		"${FILESDIR}"/${PN}-2.0.0-respect-LDFLAGS.patch \
		"${FILESDIR}"/${P}-disable-kexec-test.patch

	# to disable the -fPIE -pie in the hardened compiler
	if gcc-specs-pie ; then
		filter-flags -fPIE
		append-ldflags -nopie
	fi
}

src_configure() {
	# GNU Make's $(COMPILE.S) passes ASFLAGS to $(CCAS), CCAS=$(CC)
	export ASFLAGS="${CCASFLAGS}"
	econf \
		$(use_with booke) \
		$(use_with lzma) \
		$(use_with xen) \
		$(use_with zlib)
}

src_install() {
	default

	dodoc "${FILESDIR}"/README.Gentoo

	newinitd "${FILESDIR}"/kexec.init-${PV} kexec
	newconfd "${FILESDIR}"/kexec.conf-${PV} kexec
}
