# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="chromiumos/platform/cros_boot_mode"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit toolchain-funcs cros-debug cros-workon

DESCRIPTION="Chrome OS platform boot mode utility"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="test valgrind"

LIBCHROME_VERS="125070"

RDEPEND="test? ( chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=] )"

# qemu use isn't reflected as it is copied into the target
# from the build host environment.
DEPEND="${RDEPEND}
	test? ( dev-cpp/gmock )
	test? ( dev-cpp/gtest )
	valgrind? ( dev-util/valgrind )"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	cros-workon_src_configure
}

src_compile() {
	cros-workon_src_compile
}

src_test() {
	# Needed for `cros_run_unit_tests`.
	cros-workon_src_test
}

src_install() {
	into /
	dobin "${OUT}"/cros_boot_mode

	into /usr
	dolib.so "${OUT}"/libcros_boot_mode.so

	insinto /usr/include/cros_boot_mode
	doins \
		active_main_firmware.h \
		bootloader_type.h \
		boot_mode.h \
		developer_switch.h \
		helpers.h \
		platform_reader.h \
		platform_switch.h
}
