# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# Based on gentoo's modemmanager ebuild

EAPI="4"
CROS_WORKON_PROJECT="chromiumos/third_party/modemmanager"

inherit eutils autotools cros-workon

# ModemManager likes itself with capital letters
MY_P=${P/modemmanager/ModemManager}

DESCRIPTION="Modem and mobile broadband management libraries"
HOMEPAGE="http://mail.gnome.org/archives/networkmanager-list/2008-July/msg00274.html"
#SRC_URI not defined because we get our source locally

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE="doc"

RDEPEND=">=dev-libs/glib-2.16
        >=sys-apps/dbus-1.2
        dev-libs/dbus-glib
        net-dialup/ppp
        "

DEPEND=">=sys-fs/udev-145[gudev]
        dev-util/pkgconfig
        dev-util/intltool
        "

DOCS="AUTHORS ChangeLog NEWS README"

src_prepare() {
	eautoreconf
	intltoolize --force
}

src_configure() {
	econf $(use_with doc docs)
}

src_install() {
	default
	use doc && dohtml docs/spec.html
	# Remove useless .la files
	find "${D}" -name '*.la' -delete
	insinto /etc/init
	doins "${FILESDIR}/modemmanager.conf"
}
