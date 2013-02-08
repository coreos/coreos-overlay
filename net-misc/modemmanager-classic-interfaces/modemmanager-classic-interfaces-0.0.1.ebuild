# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# Headers so we can build apps (like cromo) that depend on the old MM
# interface (org.freedesktop.ModemManager.*) even after we've switched
# to mm-next (org.freedesktop.ModemManager1.*)

EAPI=4

DESCRIPTION="DBus interface descriptions and headers for ModemManager v0.5"
HOMEPAGE="http://www.chromium.org/"
#SRC_URI not defined because we get our source locally

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

DEPEND="!<net-misc/modemmanager-0.5"
RDEPEND="${DEPEND}"

S=${WORKDIR}

src_install() {
	insinto /usr/include/mm
	doins "${FILESDIR}"/mm-modem.h

	insinto /usr/share/dbus-1/interfaces
	doins "${FILESDIR}"/org.freedesktop.ModemManager*.xml
	doins "${FILESDIR}"/mm-*.xml
}
