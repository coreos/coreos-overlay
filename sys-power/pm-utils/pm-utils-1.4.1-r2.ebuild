# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-power/pm-utils/pm-utils-1.4.1.ebuild,v 1.10 2010/12/12 16:50:12 armin76 Exp $

EAPI=2
inherit multilib

DESCRIPTION="Suspend and hibernation utilities"
HOMEPAGE="http://pm-utils.freedesktop.org/"
SRC_URI="http://pm-utils.freedesktop.org/releases/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm ia64 ppc ppc64 sparc x86"
IUSE="alsa debug networkmanager ntp video_cards_intel video_cards_radeon"

vbetool="!video_cards_intel? ( sys-apps/vbetool )"
DEPEND="!<app-laptop/laptop-mode-tools-1.55-r1
	!<sys-power/powermgmt-base-1.31
	!>=sys-power/powermgmt-base-1.31[-pm-utils]"
RDEPEND="${DEPEND}
	sys-apps/dbus
	>=sys-apps/util-linux-2.13
	sys-power/pm-quirks
	alsa? ( media-sound/alsa-utils )
	networkmanager? ( net-misc/networkmanager )
	ntp? ( || ( net-misc/ntp net-misc/openntpd ) )
	amd64? ( ${vbetool} )
	x86? ( ${vbetool} )
	video_cards_radeon? ( app-laptop/radeontool )"

src_prepare() {
	local ignore="01grub"
	use networkmanager || ignore+=" 55NetworkManager"
	use ntp || ignore+=" 90clock"

	# this kills our usb ethernet devices
	ignore+=" disable_wol"

	# don't touch vm settings
	ignore+=" laptop-mode"

	use debug && echo 'PM_DEBUG="true"' > "${T}"/gentoo
	echo "HOOK_BLACKLIST=\"${ignore}\"" >> "${T}"/gentoo
}

src_configure() {
	econf \
		--docdir=/usr/share/doc/${PF} \
		--disable-dependency-tracking \
		--disable-doc
}

src_install() {
	emake DESTDIR="${D}" install || die
	dodoc AUTHORS ChangeLog NEWS pm/HOWTO* README* TODO || die
	doman man/*.{1,8} || die

	insinto /etc/pm/config.d
	doins "${T}"/gentoo || die
}
