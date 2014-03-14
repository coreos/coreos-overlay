# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/dbus/dbus-1.6.18-r1.ebuild,v 1.11 2014/03/12 10:10:31 ago Exp $

EAPI=5
PYTHON_COMPAT=( python2_7 )
inherit autotools eutils linux-info flag-o-matic multilib-minimal python-any-r1 systemd virtualx user

DESCRIPTION="A message bus system, a simple way for applications to talk to each other"
HOMEPAGE="http://dbus.freedesktop.org/"
SRC_URI="http://dbus.freedesktop.org/releases/dbus/${P}.tar.gz"

LICENSE="|| ( AFL-2.1 GPL-2 )"
SLOT="0"
KEYWORDS="~alpha amd64 arm hppa ia64 ~mips ppc ~ppc64 ~s390 ~sh sparc x86 ~amd64-fbsd ~x86-fbsd ~arm-linux ~x86-linux"
IUSE="debug doc selinux static-libs systemd test X"

RDEPEND=">=dev-libs/expat-2
	selinux? (
		sec-policy/selinux-dbus
		sys-libs/libselinux
		)
	systemd? ( sys-apps/systemd )
	X? (
		x11-libs/libX11
		x11-libs/libXt
		)
	abi_x86_32? (
		!<=app-emulation/emul-linux-x86-baselibs-20131008-r4
		!app-emulation/emul-linux-x86-baselibs[-abi_x86_32(-)]
	)"
DEPEND="${RDEPEND}
	virtual/pkgconfig
	doc? (
		app-doc/doxygen
		app-text/docbook-xml-dtd:4.1.2
		app-text/xmlto
		)
	test? (
		>=dev-libs/glib-2.24
		${PYTHON_DEPS}
		)"

# out of sources build dir for make check
TBD=${WORKDIR}/${P}-tests-build

pkg_setup() {
	enewgroup messagebus
	enewuser messagebus -1 -1 -1 messagebus

	use test && python-any-r1_pkg_setup

	if use kernel_linux; then
		CONFIG_CHECK="~EPOLL"
		linux-info_pkg_setup
	fi
}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-1.5.12-selinux-when-dropping-capabilities-only-include-AUDI.patch

	# Tests were restricted because of this
	sed -i \
		-e 's/.*bus_dispatch_test.*/printf ("Disabled due to excess noise\\n");/' \
		-e '/"dispatch"/d' \
		bus/test-main.c || die

	# required for asneeded patch but also for bug 263909, cross-compile so
	# don't remove eautoreconf
	eautoreconf
}

multilib_src_configure() {
	local docconf myconf

	# so we can get backtraces from apps
	case ${CHOST} in
		*-mingw*)
			# error: unrecognized command line option '-rdynamic' wrt #488036
			;;
		*)
			append-flags -rdynamic
			;;
	esac

	# libaudit is *only* used in DBus wrt SELinux support, so disable it, if
	# not on an SELinux profile.
	myconf=(
		--disable-silent-rules
		--localstatedir="${EPREFIX}/var"
		--docdir="${EPREFIX}/usr/share/doc/${PF}"
		--htmldir="${EPREFIX}/usr/share/doc/${PF}/html"
		$(use_enable static-libs static)
		$(use_enable debug verbose-mode)
		--disable-asserts
		--disable-checks
		$(use_enable selinux)
		$(use_enable selinux libaudit)
		$(use_enable kernel_linux inotify)
		$(use_enable kernel_FreeBSD kqueue)
		$(use_enable systemd)
		--disable-embedded-tests
		--disable-modular-tests
		$(use_enable debug stats)
		--with-xml=expat
		--with-session-socket-dir=/tmp
		--with-system-pid-file=/var/run/dbus.pid
		--with-system-socket=/var/run/dbus/system_bus_socket
		--with-dbus-user=messagebus
		$(use_with X x)
		"$(systemd_with_unitdir)"
		)

	if multilib_build_binaries; then
		docconf=(
			$(use_enable doc xml-docs) \
			$(use_enable doc doxygen-docs)
		)
	else
		docconf=(
			--disable-xml-docs
			--disable-doxygen-docs
		)
		myconf+=(
			--disable-selinux
			--disable-libaudit
			--disable-systemd
			--without-x

			# libxml is easier to fake since it uses pkg-config
			--with-xml=libxml
			LIBXML_CFLAGS=' '
			LIBXML_LIBS=' '
		)
	fi

	einfo "Running configure in ${BUILD_DIR}"
	ECONF_SOURCE="${S}" econf "${myconf[@]}" "${docconf[@]}"

	if multilib_build_binaries && use test; then
		mkdir "${TBD}"
		cd "${TBD}"
		einfo "Running configure in ${TBD}"
		ECONF_SOURCE="${S}" econf "${myconf[@]}" \
			$(use_enable test asserts) \
			$(use_enable test checks) \
			$(use_enable test embedded-tests) \
			$(has_version dev-libs/dbus-glib && echo --enable-modular-tests)
	fi
}

multilib_src_compile() {
	if multilib_build_binaries; then
		# after the compile, it uses a selinuxfs interface to
		# check if the SELinux policy has the right support
		use selinux && addwrite /selinux/access

		einfo "Running make in ${BUILD_DIR}"
		emake

		if multilib_build_binaries && use test; then
			cd "${TBD}"
			einfo "Running make in ${TBD}"
			emake
		fi
	else
		emake -C dbus libdbus-1.la
	fi
}

src_test() {
	cd "${TBD}"
	DBUS_VERBOSE=1 Xemake -j1 check
}

multilib_src_install() {
	if multilib_build_binaries; then
		emake DESTDIR="${D}" install
	else
		emake DESTDIR="${D}" install-pkgconfigDATA
		emake DESTDIR="${D}" -C dbus \
			install-libLTLIBRARIES install-dbusincludeHEADERS \
			install-nodist_dbusarchincludeHEADERS
	fi
}

multilib_src_install_all() {
	newinitd "${FILESDIR}"/dbus.initd dbus

	if use X; then
		# dbus X session script (#77504)
		# turns out to only work for GDM (and startx). has been merged into
		# other desktop (kdm and such scripts)
		exeinto /etc/X11/xinit/xinitrc.d
		doexe "${FILESDIR}"/80-dbus
	fi

	# needs to exist for dbus sessions to launch
	keepdir /usr/share/dbus-1/services
	keepdir /etc/dbus-1/{session,system}.d
	# machine-id symlink from pkg_postinst()
	keepdir /var/lib/dbus

	dodoc AUTHORS ChangeLog HACKING NEWS README doc/TODO
	prune_libtool_files --all
}

pkg_postinst() {
	if [ "$(rc-config list default | grep dbus)" = "" ] ; then
		elog "To start the D-Bus system-wide messagebus by default"
		elog "you should add it to the default runlevel :"
		elog "\`rc-update add dbus default\`"
		elog
	fi

	elog "Some applications require a session bus in addition to the system"
	elog "bus. Please see \`man dbus-launch\` for more information."
	elog

	if [ "$(rc-status | grep dbus | grep started)" ] ; then
		ewarn "You must restart D-Bus \`/etc/init.d/dbus restart\` to run"
		ewarn "the new version of the daemon."
		ewarn "Don't do this while X is running because it will restart your X as well."
	fi

	# Ensure unique id is generated and put it in /etc wrt #370451 but symlink
	# for DBUS_MACHINE_UUID_FILE (see tools/dbus-launch.c) and reverse
	# dependencies with hardcoded paths (although the known ones got fixed already)
	dbus-uuidgen --ensure="${EROOT}"/etc/machine-id
	ln -sf "${EROOT}"/etc/machine-id "${EROOT}"/var/lib/dbus/machine-id
}
