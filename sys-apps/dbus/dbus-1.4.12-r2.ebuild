# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/dbus/dbus-1.4.12.ebuild,v 1.7 2011/06/26 10:44:46 armin76 Exp $

EAPI=2
inherit autotools eutils multilib flag-o-matic python systemd virtualx

DESCRIPTION="A message bus system, a simple way for applications to talk to each other"
HOMEPAGE="http://dbus.freedesktop.org/"
SRC_URI="http://dbus.freedesktop.org/releases/dbus/${P}.tar.gz"

LICENSE="|| ( GPL-2 AFL-2.1 )"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ~ppc64 s390 sh sparc x86 ~x86-fbsd"
IUSE="debug doc selinux static-libs test X"

RDEPEND="
	X? (
		x11-libs/libX11
		x11-libs/libXt
	)
	selinux? (
		sys-libs/libselinux
		sec-policy/selinux-dbus
	)
	>=dev-libs/expat-1.95.8
"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	doc? (
		app-doc/doxygen
		app-text/docbook-xml-dtd:4.1.2
		app-text/xmlto
	)
	test? (
		=dev-lang/python-2*
		>=dev-libs/glib-2.22:2
	)
"

# out of sources build directory
BD=${WORKDIR}/${P}-build
# out of sources build dir for make check
TBD=${WORKDIR}/${P}-tests-build

pkg_setup() {
	enewgroup messagebus
	enewuser messagebus -1 "-1" -1 messagebus

	if use test; then
		python_set_active_version 2
		python_pkg_setup
	fi
}

src_prepare() {
	# Tests were restricted because of this
	sed -i \
		-e 's/.*bus_dispatch_test.*/printf ("Disabled due to excess noise\\n");/' \
		-e '/"dispatch"/d' \
		bus/test-main.c || die

	epatch "${FILESDIR}"/${PN}-1.4.0-asneeded.patch

	epatch "${FILESDIR}"/${P}-send-print-fixed.patch
	epatch "${FILESDIR}"/${P}-send-unix-fd.patch

	# required for asneeded patch but also for bug 263909, cross-compile so
	# don't remove eautoreconf
	eautoreconf
}

src_configure() {
	local my_conf

	# so we can get backtraces from apps
	append-flags -rdynamic

	# libaudit is *only* used in DBus wrt SELinux support, so disable it, if
	# not on an SELinux profile.
	my_conf="$(use_with X x)
		$(use_enable debug verbose-mode)
		$(use_enable debug asserts)
		$(use_enable kernel_linux inotify)
		$(use_enable kernel_FreeBSD kqueue)
		$(use_enable selinux)
		$(use_enable selinux libaudit)
		$(use_enable static-libs static)
		--enable-shared
		--with-xml=expat
		--with-system-pid-file=/var/run/dbus.pid
		--with-system-socket=/var/run/dbus/system_bus_socket
		--with-session-socket-dir=/tmp
		--with-dbus-user=messagebus
		$(systemd_with_unitdir)
		--localstatedir=/var"

	mkdir "${BD}"
	cd "${BD}"
	einfo "Running configure in ${BD}"
	ECONF_SOURCE="${S}" econf ${my_conf} \
		$(use_enable doc doxygen-docs) \
		$(use_enable doc xml-docs)

	if use test; then
		local circular
		if ! has_version dev-libs/dbus-glib; then
			circular="--disable-modular-tests"
			ewarn "Skipping modular tests because dev-libs/dbus-glib is missing"
		fi

		mkdir "${TBD}"
		cd "${TBD}"
		einfo "Running configure in ${TBD}"
		ECONF_SOURCE="${S}" econf \
			${my_conf} \
			$(use_enable test checks) \
			$(use_enable test embedded-tests) \
			$(use_enable test modular-tests) \
			$(use_enable test asserts) \
			${circular}
	fi
}

src_compile() {
	# after the compile, it uses a selinuxfs interface to
	# check if the SELinux policy has the right support
	use selinux && addwrite /selinux/access

	cd "${BD}"
	einfo "Running make in ${BD}"
	emake || die

	if use doc; then
		doxygen || die
	fi

	if use test; then
		cd "${TBD}"
		einfo "Running make in ${TBD}"
		emake || die
	fi
}

src_test() {
	cd "${TBD}"
	DBUS_VERBOSE=1 Xemake -j1 check || die
}

src_install() {
	# initscript
	newinitd "${FILESDIR}"/dbus.init-1.0 dbus || die

	if use X; then
		# dbus X session script (#77504)
		# turns out to only work for GDM (and startx). has been merged into
		# other desktop (kdm and such scripts)
		exeinto /etc/X11/xinit/xinitrc.d/
		doexe "${FILESDIR}"/80-dbus || die
	fi

	# needs to exist for the system socket
	keepdir /var/run/dbus

	# needs to exist for dbus sessions to launch
	keepdir /usr/lib/dbus-1.0/services
	keepdir /usr/share/dbus-1/services
	keepdir /etc/dbus-1/system.d/
	keepdir /etc/dbus-1/session.d/

	dodoc AUTHORS ChangeLog HACKING NEWS README doc/TODO || die

	cd "${BD}"
	# FIXME: split dtd's in dbus-dtd ebuild
	emake DESTDIR="${D}" install || die
	if use doc; then
		dohtml -p api/ doc/api/html/* || die
		cd "${S}"
		dohtml doc/*.html || die
	fi

	# Remove .la files
	find "${D}" -type f -name '*.la' -exec rm -f {} +
}

pkg_postinst() {
	elog "To start the D-Bus system-wide messagebus by default"
	elog "you should add it to the default runlevel :"
	elog "\`rc-update add dbus default\`"
	elog
	elog "Some applications require a session bus in addition to the system"
	elog "bus. Please see \`man dbus-launch\` for more information."
	elog
	ewarn "You must restart D-Bus \`/etc/init.d/dbus restart\` to run"
	ewarn "the new version of the daemon."
	ewarn "Don't do this while X is running because it will restart your X as well."

	# Move to /etc per #370451 and ensure unique id is generated
	[[ -e ${ROOT}/var/lib/dbus/machine-id ]] && \
		mv -vf "${ROOT}"/var/lib/dbus/machine-id "${ROOT}"/etc/machine-id
	dbus-uuidgen --ensure="${ROOT}"/etc/machine-id
}
