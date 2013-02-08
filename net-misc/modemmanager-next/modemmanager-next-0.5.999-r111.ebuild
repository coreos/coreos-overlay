# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# Based on gentoo's modemmanager ebuild

EAPI="4"
CROS_WORKON_COMMIT="586918c901cc6c95e136c2371f237666b9e20136"
CROS_WORKON_TREE="8d36420a379eddc26d8e562dea0b00081807fdae"
CROS_WORKON_PROJECT="chromiumos/third_party/modemmanager-next"

inherit eutils autotools cros-workon

# ModemManager likes itself with capital letters
MY_P=${P/modemmanager/ModemManager}

DESCRIPTION="Modem and mobile broadband management libraries"
HOMEPAGE="http://mail.gnome.org/archives/networkmanager-list/2008-July/msg00274.html"
#SRC_URI not defined because we get our source locally

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="doc qmi"

RDEPEND=">=dev-libs/glib-2.32
	>=sys-apps/dbus-1.2
	dev-libs/dbus-glib
	net-dialup/ppp
	qmi? ( net-libs/libqmi )
	!net-misc/modemmanager"

DEPEND="${RDEPEND}
	>=sys-fs/udev-145[gudev]
	dev-util/pkgconfig
	dev-util/intltool
	>=dev-util/gtk-doc-1.13
	!net-misc/modemmanager-next-interfaces
	!net-misc/modemmanager"

DOCS="AUTHORS ChangeLog NEWS README"

src_prepare() {
	gtkdocize
	eautopoint
	eautoreconf
	intltoolize --force
}

src_configure() {
	econf \
		--with-html-dir="\${datadir}/doc/${PF}/html" \
		$(use_with doc docs) \
		$(use_with qmi)
}

src_install() {
	default
	# Remove useless .la files
	find "${D}" -name '*.la' -delete

	# WORKAROUND: Workaround a race condition with modemmanager-next
	# failing to initially blacklist a device.  This causes problems with
	# shill because there is a short time period where both mm-next and
	# cromo are reporting the modem to shill.  Shill doesn't like multiple
	# devices with the same name.
	#
	# The main problem here is that mm-next determines whether a tty
	# interface should be blacklisted by looking at the parent usb device
	# ID_MM_DEVICE_IGNORE property.  This property is set by a udev rule
	# for add/change kernel uevents.  This check works if udev processes
	# the parent usb device before the child tty interface.  However, udev
	# only ensures that the parent is processed before the child for add
	# events, it does not guarantee such ordering for change events.
	# In the case where the kernel fires the add event before udev is
	# started, udev does not get to see the add event.  Instead, the udev
	# rule sets the ID_MM_DEVICE_IGNORE property when it sees a change
	# event (which doesn't guarantee parent/child ordering).  This leads
	# to mm-next failing to blacklist a modem because the property
	# ID_MM_DEVICE_IGNORE has not been set.
	#
	# To workaround this problem, we remove all plugins that we do not
	# support in mm-next (especially the gobi plugin since cromo is what
	# we use to manage the gobi modems and we do not want both cromo and
	# mm-next to manage the same modem at the same time).
	# crosbug.com/33719
	#
	# TODO(thieule): Renable all plugins after we deprecate cromo.
	# crosbug.com/33930
	if ! use qmi; then
		find "${D}" -name 'libmm-plugin-*.so' ! \
			\( -name 'libmm-plugin-samsung.so' -o \
			   -name 'libmm-plugin-huawei.so' -o \
			   -name 'libmm-plugin-novatel-lte.so' \) \
			-delete
	fi

	insinto /etc/init
	doins "${FILESDIR}/modemmanager.conf"
}
