# Copyright 2010 Chromium OS Authors
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils toolchain-funcs

DESCRIPTION="Superiotool allows you to detect which Super I/O you have on your mainboard, and it can provide detailed information about the register contents of the Super I/O."
HOMEPAGE="http://www.coreboot.org/Superiotool"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${PN}-svn-${PV}.tar.bz2"

S=${PN}

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86"

RDEPEND="sys-apps/pciutils"
DEPEND="${RDEPEND}"

src_compile() {
	cd ${S}
	emake CC="$(tc-getCC)" || die "emake failed"
}

src_install() {
	cd ${S}
	emake DESTDIR="${D}" install || die
}
