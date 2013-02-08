# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-wireless/iw/iw-3.1.ebuild,v 1.3 2011/10/27 16:03:02 jer Exp $

EAPI="2"

inherit toolchain-funcs eutils

DESCRIPTION="nl80211-based configuration utility for wireless devices using the mac80211 kernel stack"
HOMEPAGE="http://wireless.kernel.org/en/users/Documentation/iw"
SRC_URI="http://linuxwireless.org/download/${PN}/${P}.tar.bz2"

LICENSE="as-is"
SLOT="0"
KEYWORDS="amd64 arm ~ppc x86 ~amd64-linux ~x86-linux"
IUSE=""

RDEPEND=">=dev-libs/libnl-1.1"
DEPEND="${RDEPEND}
	dev-util/pkgconfig"

src_prepare() {
	epatch "${FILESDIR}/${P}-nl80211.patch"
	epatch "${FILESDIR}/${P}-beacon-loss.patch"
	tc-export CC LD
}

src_install() {
	emake install DESTDIR="${D}" || die
}
