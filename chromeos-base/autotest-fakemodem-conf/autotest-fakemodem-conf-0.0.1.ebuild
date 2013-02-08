# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

DESCRIPTION="Runtime configuration file for fakemodem (autotest dep)"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

src_install() {
	insinto /etc/dbus-1/system.d
	doins "${FILESDIR}/org.chromium.FakeModem.conf" || die
}
