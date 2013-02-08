# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"
CROS_WORKON_PROJECT="chromiumos/platform/vpn-manager"
CROS_WORKON_OUTOFTREE_BUILD=1

inherit cros-debug cros-workon multilib

DESCRIPTION="VPN tools"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

LIBCHROME_VERS="125070"

RDEPEND="chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	 chromeos-base/libchromeos
	 dev-cpp/gflags
	 dev-libs/openssl
	 net-dialup/xl2tpd
	 net-misc/strongswan[cisco,nat-transport]"
DEPEND="${RDEPEND}
	 dev-cpp/gtest"

src_prepare() {
	cros-workon_src_prepare
}

src_configure() {
	cros-workon_src_configure
	export LIBDIR="/usr/$(get_libdir)"
}

src_compile() {
	cros-workon_src_compile
}

src_test() {
	if ! use x86 && ! use amd64 ; then
		echo Skipping unit tests on non-x86 platform
	else
		cros-workon_src_test
	fi
}

src_install() {
	dosbin "${OUT}"/l2tpipsec_vpn
	exeinto /usr/libexec/l2tpipsec_vpn
	doexe "bin/pluto_updown"
}
