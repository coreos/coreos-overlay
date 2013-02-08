# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-wireless/crda/crda-1.1.1.ebuild,v 1.1 2010/01/26 17:02:57 chainsaw Exp $

EAPI="2"

inherit toolchain-funcs multilib

DESCRIPTION="Central Regulatory Domain Agent for wireless networks."
HOMEPAGE="http://wireless.kernel.org/en/developers/Regulatory"
SRC_URI="http://wireless.kernel.org/download/crda/${P}.tar.bz2"
LICENSE="as-is"
SLOT="0"

KEYWORDS="amd64 arm ppc x86 ~amd64-linux ~x86-linux"
IUSE=""
RDEPEND="dev-libs/libgcrypt
	dev-libs/libnl:0
	net-wireless/wireless-regdb"
DEPEND="${RDEPEND}
	dev-python/m2crypto"

src_prepare() {
	##Make sure we install the rules where udev rules go...
	sed -i -e "/^UDEV_RULE_DIR/s:lib:$(get_libdir):" "${S}"/Makefile || \
	    die "Makefile sed failed"

	# install version that also handles "add" events
	cp -f "${FILESDIR}"/regulatory.rules "${S}"/udev || \
	    die "Failed to install new regulatory.rules"

	# Make sure we hit the correct pkg-config wrapper
	sed -i \
		-e "s:\<pkg-config\>:$(tc-getPKG_CONFIG):" \
		"${S}"/Makefile || die
}

src_compile() {
	#
	# NB: crda assumes regdbdump built for the target can run on
	#     the build host which doesn't work; use all_noverify as
	#     a WAR
	emake CC="$(tc-getCC)" all_noverify || die "Compilation failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
}
