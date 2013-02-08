# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="chromiumos/third_party/memtest"

DESCRIPTION="The memtest86 memory tester"
HOMEPAGE="http://www.memtest86.com"
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"

RDEPEND=""
DEPEND=""

CROS_WORKON_LOCALNAME="memtest"

inherit cros-workon toolchain-funcs

src_compile() {
	local prefix="i686-pc-linux-gnu"
	emake -j1 AS=${prefix}-as CC=${prefix}-gcc LD=${prefix}-ld.bfd
}

src_install() {
	insinto "/firmware"
	newins memtest x86-memtest
}
