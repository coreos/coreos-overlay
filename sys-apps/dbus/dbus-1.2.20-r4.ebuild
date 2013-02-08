# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/dbus/dbus-1.2.12.ebuild,v 1.3 2009/04/23 05:46:44 nirbheek Exp $

inherit eutils multilib flag-o-matic

DESCRIPTION="A message bus system, a simple way for applications to talk to each other"
HOMEPAGE="http://dbus.freedesktop.org/"
SRC_URI="http://dbus.freedesktop.org/releases/dbus/${P}.tar.gz"

LICENSE="|| ( GPL-2 AFL-2.1 )"
SLOT="0"
KEYWORDS="~alpha ~amd64 ~arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc ~x86 ~x86-fbsd"
IUSE="debug doc selinux test X"

RDEPEND="X? ( x11-libs/libXt x11-libs/libX11 )
	selinux? ( sys-libs/libselinux
				sec-policy/selinux-dbus )
	>=dev-libs/expat-1.95.8
	!<sys-apps/dbus-0.91"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	doc? (	app-doc/doxygen
		app-text/xmlto )"

src_unpack() {
	unpack ${A}
	cd "${S}"

	# Tests were restricted because of this
	sed -e 's/.*bus_dispatch_test.*/printf ("Disabled due to excess noise\\n");/' \
		-e '/"dispatch"/d' -i "${S}/bus/test-main.c"
}

src_compile() {
	# so we can get backtraces from apps
	append-flags -rdynamic

	epatch "${FILESDIR}"/${PN}-gold.patch

	epatch "${FILESDIR}"/dbus-send-print-fixed.patch

	# libaudit is *only* used in DBus wrt SELinux support, so disable it, if
	# not on an SELinux profile.
	econf \
		$(use_with X x) \
		$(use_enable kernel_linux inotify) \
		$(use_enable kernel_FreeBSD kqueue) \
		$(use_enable selinux) \
		$(use_enable selinux libaudit)	\
		$(use_enable debug verbose-mode) \
		$(use_enable debug asserts) \
		$(use_enable test tests) \
		$(use_enable test asserts) \
		--with-xml=expat \
		--with-system-pid-file=/var/run/dbus.pid \
		--with-system-socket=/var/run/dbus/system_bus_socket \
		--with-session-socket-dir=/tmp \
		--with-dbus-user=messagebus \
		--localstatedir=/var \
		$(use_enable doc doxygen-docs) \
		--disable-xml-docs \
		|| die "econf failed"

	# after the compile, it uses a selinuxfs interface to
	# check if the SELinux policy has the right support
	use selinux && addwrite /selinux/access

	emake || die "make failed"
}

src_test() {
	DBUS_VERBOSE=1 make check || die "make check failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "make install failed"

	# initscript
	newinitd "${FILESDIR}"/dbus.init-1.0 dbus

	if use X ; then
		# dbus X session script (#77504)
		# turns out to only work for GDM. has been merged into other desktop
		# (kdm and such scripts)
		exeinto /etc/X11/xinit/xinitrc.d/
		doexe "${FILESDIR}"/30-dbus
	fi

	# needs to exist for the system socket
	keepdir /var/run/dbus
	# needs to exist for machine id
	keepdir /var/lib/dbus
	# needs to exist for dbus sessions to launch

	keepdir /usr/lib/dbus-1.0/services
	keepdir /usr/share/dbus-1/services
	keepdir /etc/dbus-1/system.d/
	keepdir /etc/dbus-1/session.d/

	# TODO(petkov): See crosbug.com/p/2264 -- remove when fixed.
	dosym /usr/bin/dbus-uuidgen /bin/dbus-uuidgen
	dosym /usr/bin/dbus-daemon /bin/dbus-daemon

	dodoc AUTHORS ChangeLog HACKING NEWS README doc/TODO
	if use doc; then
		dohtml doc/*html
	fi
}

pkg_preinst() {
	enewgroup messagebus
	enewuser messagebus -1 "-1" -1 messagebus
}

pkg_postinst() {
	elog "To start the D-Bus system-wide messagebus by default"
	elog "you should add it to the default runlevel :"
	elog "\`rc-update add dbus default\`"
	elog
	elog "Some applications require a session bus in addition to the system"
	elog "bus. Please see \`man dbus-launch\` for more information."
	elog
	elog
	ewarn "You MUST run 'revdep-rebuild' after emerging this package"
	elog
	ewarn "You must restart D-Bus \`/etc/init.d/dbus restart\` to run"
	ewarn "the new version of the daemon."

	if has_version x11-base/xorg-server && built_with_use x11-base/xorg-server hal; then
		elog
		ewarn "You are currently running X with the hal useflag enabled"
		ewarn "restarting the dbus service WILL restart X as well"
		ebeep 5
	fi

	if use test; then
		elog
		ewarn "You have unit tests enabled, this results in an insecure library"
		ewarn "It is recommended that you reinstall *without* FEATURES=test"
	fi
}
