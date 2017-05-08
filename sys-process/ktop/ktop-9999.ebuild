#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
# $Header:$
#

EAPI=2
CROS_WORKON_PROJECT="chromiumos/third_party/ktop"
inherit epatch toolchain-funcs cros-workon

DESCRIPTION="Utility for looking at top users of system calls"
HOMEPAGE="http://git.chromium.org/gitweb/?s=ktop"
SRC_URI=""

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~arm ~arm64 ~amd64 ~x86"
IUSE=""

DEPEND="sys-libs/ncurses"

src_compile() {
	tc-export CC
	emake || die
}

src_install() {
	emake install DESTDIR="${D}" sbindir=/usr/sbin || die
}
