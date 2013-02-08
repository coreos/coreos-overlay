# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_COMMIT="d73783fb016a4c9d7d8644db83c8caeb447b0063"
CROS_WORKON_TREE="e68fbfeee03d9d1e0e4a248b13d7081eaaacf317"
CROS_WORKON_PROJECT="chromiumos/platform/shill"

inherit cros-debug cros-workon toolchain-funcs multilib

DESCRIPTION="Shill Connection Manager for Chromium OS"
HOMEPAGE="http://src.chromium.org"
LICENSE="BSD"
SLOT="0"
IUSE="test"
KEYWORDS="amd64 arm x86"

RDEPEND="chromeos-base/bootstat
	chromeos-base/chromeos-minijail
	!<chromeos-base/flimflam-0.0.1-r530
	chromeos-base/libchrome:125070[cros-debug=]
	chromeos-base/libchromeos
	chromeos-base/metrics
	>=chromeos-base/mobile-providers-0.0.1-r12
	chromeos-base/vpn-manager
	dev-libs/dbus-c++
	>=dev-libs/glib-2.30
	dev-libs/libnl:3
	dev-libs/nss
	dev-libs/protobuf
	net-dialup/ppp
	net-dns/c-ares
	net-misc/dhcpcd
	net-misc/openvpn
	net-wireless/wpa_supplicant[dbus]"

DEPEND="${RDEPEND}
	chromeos-base/system_api
	chromeos-base/wimax_manager
	test? ( dev-cpp/gmock )
	test? ( dev-cpp/gtest )
	virtual/modemmanager"

make_flags() {
	echo LIBDIR="/usr/$(get_libdir)"
}

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG

	emake $(make_flags) shill shims
}

src_test() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	cros-debug-add-NDEBUG

	# Build tests
	emake $(make_flags) shill_unittest

	# Run tests if we're on x86
	if ! use x86 && ! use amd64 ; then
		echo Skipping tests on non-x86/amd64 platform...
	else
		for ut in shill ; do
			"${S}/${ut}_unittest" \
				${GTEST_ARGS} || die "${ut}_unittest failed"
		done
	fi
}

src_install() {
	dobin bin/ff_debug
	dobin bin/mm_debug
	dobin bin/set_apn
	dobin bin/set_arpgw
	dobin bin/shill_login_user
	dobin bin/shill_logout_user
	dobin bin/wpa_debug
	dobin shill
	local shims_dir="/usr/$(get_libdir)/shill/shims"
	exeinto "${shims_dir}"
	doexe build/shims/net-diags-upload
	doexe build/shims/nss-get-cert
	doexe build/shims/openvpn-script
	doexe build/shims/set-apn-helper
	doexe build/shims/shill-pppd-plugin.so
	insinto "${shims_dir}"
	doins build/shims/wpa_supplicant.conf
	insinto /etc
	doins shims/nsswitch.conf
	dosym /var/run/shill/resolv.conf /etc/resolv.conf
	insinto /etc/dbus-1/system.d
	doins shims/org.chromium.flimflam.conf
	insinto /usr/share/shill
	doins data/cellular_operator_info
	# Install introspection XML
	insinto /usr/share/dbus-1/interfaces
	doins dbus_bindings/org.chromium.flimflam.*.xml
}
