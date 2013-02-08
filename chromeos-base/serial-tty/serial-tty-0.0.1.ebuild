# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

inherit cros-serialuser

DESCRIPTION="Init script to run agetty on the serial port"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND="
	!chromeos-base/tegra-debug
	!chromeos-base/serial-sac-tty
"
RDEPEND="
	${DEPEND}
	sys-apps/upstart
"

# Because this ebuild has no source package, "${S}" doesn't get
# automatically created.  The compile phase depends on "${S}" to
# exist, so we make sure "${S}" refers to a real directory.
#
# The problem is apparently an undocumented feature of EAPI 4;
# earlier versions of EAPI don't require this.
S="${WORKDIR}"

# To compile, just replace %PORT% in our conf file with the port provided
# by cros-serialuser...
src_compile() {
	sed -e "s|%PORT%|$(get_serial_name)|" \
		"${FILESDIR}"/tty-serial.conf \
		> tty-serial.conf || die
}

src_install() {
	insinto /etc/init
	doins tty-serial.conf
}
