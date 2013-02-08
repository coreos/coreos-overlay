# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
inherit toolchain-funcs

DESCRIPTION="App that disables ECHO on tty1 for Chromium OS"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

S="${WORKDIR}"

src_unpack() {
	cp -R "${FILESDIR}"/* ./ || die
}

src_compile() {
	tc-export CC
	emake
}

src_install() {
	into /
	dosbin disable_echo

	insinto /etc/init
	doins disable_echo.conf
}
