# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4
CROS_WORKON_PROJECT="chromiumos/platform/debugd"
CROS_WORKON_LOCALNAME=$(basename ${CROS_WORKON_PROJECT})

inherit cros-debug cros-workon toolchain-funcs

DESCRIPTION="Chrome OS debugging service"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""

LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

LIBCHROME_VERS="125070"

RDEPEND="chromeos-base/chromeos-minijail
	chromeos-base/libchrome:${LIBCHROME_VERS}[cros-debug=]
	chromeos-base/libchromeos
	dev-libs/dbus-c++
	dev-libs/glib:2
	dev-libs/libpcre"
DEPEND="${RDEPEND}
	chromeos-base/shill
	sys-apps/dbus
	virtual/modemmanager"

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG OBJCOPY
	cros-debug-add-NDEBUG
	emake BASE_VER=${LIBCHROME_VERS}
}

src_test() {
	emake tests BASE_VER=${LIBCHROME_VERS}
}

src_install() {
	cd build-opt
	into /
	dosbin debugd
	dodir /debugd
	exeinto /usr/libexec/debugd/helpers
	doexe helpers/icmp
	doexe helpers/netif
	doexe helpers/modem_status
	doexe "${S}"/src/helpers/minijail-setuid-hack.sh
	doexe "${S}"/src/helpers/systrace.sh
	doexe helpers/network_status

	insinto /etc/dbus-1/system.d
	doins "${FILESDIR}/org.chromium.debugd.conf"

	insinto /etc/init
	doins "${FILESDIR}/debugd.conf"
	doins "${FILESDIR}/trace_marker-test.conf"
}
