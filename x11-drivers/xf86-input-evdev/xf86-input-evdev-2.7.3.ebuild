# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-drivers/xf86-input-evdev/xf86-input-evdev-2.7.3.ebuild,v 1.1 2012/08/14 01:24:15 chithanh Exp $

EAPI=4
XORG_EAUTORECONF=yes

inherit xorg-2

DESCRIPTION="Generic Linux input driver"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~sh ~sparc x86"
IUSE=""
RDEPEND=">=x11-base/xorg-server-1.10[udev]
	sys-libs/mtdev"
DEPEND="${RDEPEND}
	>=x11-proto/inputproto-2.1.99.3
	>=sys-kernel/linux-headers-2.6"

PATCHES=(
	"${FILESDIR}"/evdev-2.7.0-Use-monotonic-timestamps-for-input-events-if-availab.patch
        # crosbug.com/35291
	"${FILESDIR}"/evdev-2.7.3-Add-SYN_DROPPED-handling.patch
	"${FILESDIR}/evdev-disable-smooth-scrolling.patch"
	"${FILESDIR}/evdev-2.6.99-wheel-accel.patch"
	"${FILESDIR}"/evdev-2.7.0-feedback-log.patch
	"${FILESDIR}"/evdev-2.7.0-add-touch-event-timestamp.patch
	# crosbug.com/p/13787
	"${FILESDIR}"/evdev-2.7.0-fix-emulated-wheel.patch
	"${FILESDIR}"/evdev-2.7.0-add-block-reading-support.patch
)
