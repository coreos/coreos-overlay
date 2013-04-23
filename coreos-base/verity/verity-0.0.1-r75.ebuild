# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_COMMIT="658d35ef6f15cb7763ce4bdd03161837452de7cd"
CROS_WORKON_TREE="a54ff06d13ee681a7cb660ed14c51dcd8ef5ffdf"
CROS_WORKON_PROJECT="chromiumos/platform/dm-verity"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-workon cros-au

DESCRIPTION="File system integrity image generator for Chromium OS"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE="32bit_au test valgrind splitdebug"

RDEPEND=""

# qemu use isn't reflected as it is copied into the target
# from the build host environment.
DEPEND="${RDEPEND}
	dev-cpp/gtest
	dev-cpp/gmock
	32bit_au? (
		dev-cpp/gtest32
		dev-cpp/gmock32
	)
	valgrind? ( dev-util/valgrind )"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	use 32bit_au && board_setup_32bit_au_env
	cros-workon_src_configure
}

src_compile() {
	cros-workon_src_compile
}

src_test() {
	cros-workon_src_test
}

src_install() {
	cros-workon_src_install
	dolib.a "${OUT}"/libdm-bht.a
	insinto /usr/include/verity
	doins dm-bht.h dm-bht-userspace.h
	insinto /usr/include/verity
	cd include
	doins -r linux asm asm-generic crypto
	cd ..
	into /
	dobin "${OUT}"/verity-static
	dosym verity-static bin/verity
}
