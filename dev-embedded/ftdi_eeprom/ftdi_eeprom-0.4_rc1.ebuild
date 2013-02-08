# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-embedded/ftdi_eeprom/ftdi_eeprom-0.3.ebuild,v 1.1 2010/06/22 22:19:02 vapier Exp $

EAPI="2"

inherit autotools

DESCRIPTION="Utility to program external EEPROM for FTDI USB chips"
HOMEPAGE="http://www.intra2net.com/en/developer/libftdi/"
#
# from HEAD 08d3572 'Support for FT232R eeprom features'
#
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${PN}-${PV}.tar.gz"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="x86 amd64"
IUSE=""

DEPEND=">=dev-embedded/libftdi-0.19
	dev-libs/confuse"

src_prepare() {
	epatch ${FILESDIR}/getopts.patch || die "patching getopts.patch"
	eautoreconf      
}

src_install() {
	emake DESTDIR="${D}" install || die
	insinto "/usr/share/ftdi_eeprom"
	for item in ${FILESDIR}/confs/*.conf; do
		doins ${item}
	done
	dodoc AUTHORS ChangeLog README src/example.conf
}
