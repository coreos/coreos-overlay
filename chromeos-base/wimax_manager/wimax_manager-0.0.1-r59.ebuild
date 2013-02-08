# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_COMMIT="0b9ac7ec74ba38ffa42d7f04b5c739fb5e001187"
CROS_WORKON_TREE="4ebbad4742c5c0ec59e6499688750fc6a15c2790"
CROS_WORKON_PROJECT="chromiumos/platform/wimax_manager"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-debug cros-workon

DESCRIPTION="Chromium OS WiMAX Manager"
HOMEPAGE="http://www.chromium.org/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="gdmwimax test"

LIBCHROME_VERS="125070"

RDEPEND="gdmwimax? (
	chromeos-base/libchromeos
	chromeos-base/metrics
	dev-libs/dbus-c++
	>=dev-libs/glib-2.30
	dev-libs/protobuf
)"

DEPEND="gdmwimax? (
	${RDEPEND}
	chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	chromeos-base/system_api
	net-wireless/gdmwimax
	test? ( dev-cpp/gmock )
	test? ( dev-cpp/gtest )
)"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	cros-workon_src_configure
}

src_compile() {
	use gdmwimax || return 0
	cros-workon_src_compile
}

src_test() {
	use gdmwimax || return 0

	# Needed for `cros_run_unit_tests`.
	cros-workon_src_test
}

src_install() {
	# Install D-Bus introspection XML files.
	insinto /usr/share/dbus-1/interfaces
	doins dbus_bindings/org.chromium.WiMaxManager*.xml

	# Skip the rest of the files unless USE=gdmwimax is specified.
	use gdmwimax || return 0

	# Install daemon executable.
	dosbin "${OUT}"/wimax-manager

	# Install upstart config file.
	insinto /etc/init
	doins wimax_manager.conf

	# Install D-Bus config file.
	insinto /etc/dbus-1/system.d
	doins dbus_bindings/org.chromium.WiMaxManager.conf
}
