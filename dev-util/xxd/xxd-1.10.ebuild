# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI="4"

inherit toolchain-funcs

DESCRIPTION="make a hexdump or do the reverse"
HOMEPAGE="http://ftp.uni-erlangen.de/pub/utilities/etc/?order=s"
SRC_URI="http://ftp.uni-erlangen.de/pub/utilities/etc/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

src_prepare() {
	# use implicit make rules as they're better than the makefile
	echo 'all: xxd' > Makefile
	tc-export CC
}

src_install() {
	# Has to be /bin rather than /usr/bin due to conflict with vim
	into /
	dobin xxd
}
