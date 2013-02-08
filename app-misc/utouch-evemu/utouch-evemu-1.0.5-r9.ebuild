# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2

inherit base

DESCRIPTION="To record and replay data from kernel evdev devices"
HOMEPAGE="http://bitmath.org/code/evemu/"
SRC_URI="http://launchpad.net/utouch-evemu/trunk/v${PV}/+download/${P}.tar.gz"
LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE="X"

RDEPEND="X? ( >=x11-base/xorg-server-1.8 )"
DEPEND="${RDEPEND}"

PATCHES=(
        "${FILESDIR}/1.0.5-signal_exit.patch"
        "${FILESDIR}/1.0.5-timeout.patch"
        "${FILESDIR}/1.0.5-play_timestamp.patch"
        "${FILESDIR}/1.0.5-add_slot0.patch"
        "${FILESDIR}/1.0.5-getopt.patch"
        "${FILESDIR}/1.0.5-fix_init_absinfo.patch"
        "${FILESDIR}/1.0.5-add_resolution.patch"
        "${FILESDIR}/1.0.5-raw_access_api.patch"
)

src_install() {
        emake DESTDIR="${D}" install || die "install failed"
}
