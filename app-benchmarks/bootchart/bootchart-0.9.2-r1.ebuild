# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# NOTE: This is based on the bootchart found in Ubuntu, which is a re-working
# of the bootchart project to use a C-based collector daemon. There wasn't a
# good link to a source tarball to use in the ebuild and all we need are the
# collector and gather files from it so they are inlined in the FILESDIR.

inherit toolchain-funcs

DESCRIPTION="Performance analysis and visualization of the system boot process"
HOMEPAGE="http://packages.ubuntu.com/lucid/bootchart"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND=""
RDEPEND=""

src_unpack() {
	mkdir "${S}"
	cp "${FILESDIR}/bootchart-collector.c" "${S}/collector.c"
	cp "${FILESDIR}/bootchart-gather.sh" "${S}/gather"
	cp "${FILESDIR}/bootchart.conf" "${S}"
}

src_compile() {
	$(tc-getCC) ${CFLAGS} -o collector collector.c ||
		die "Unable to compile bootchart collector."
}

src_install() {
	exeinto /lib/bootchart
	doexe collector gather

	insinto /etc/init
	doins bootchart.conf
}
