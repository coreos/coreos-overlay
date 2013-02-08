# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/glib/glib-2.30.0.ebuild,v 1.2 2011/09/28 11:19:21 ssuominen Exp $

EAPI="4"

inherit autotools gnome.org libtool eutils flag-o-matic multilib pax-utils virtualx

DESCRIPTION="The GLib library of C routines"
HOMEPAGE="http://www.gtk.org/"
SRC_URI="${SRC_URI}
	http://pkgconfig.freedesktop.org/releases/pkg-config-0.26.tar.gz" # pkg.m4 for eautoreconf

LICENSE="LGPL-2"
SLOT="2"
IUSE="debug doc fam selinux +static-libs systemtap test xattr"
KEYWORDS="~alpha ~amd64 ~arm ~hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh
~sparc ~x86 ~sparc-fbsd ~x86-fbsd ~x86-linux"

RDEPEND="virtual/libiconv
	virtual/libffi
	sys-libs/zlib
	xattr? ( sys-apps/attr )
	fam? ( virtual/fam )"
DEPEND="${RDEPEND}
	>=sys-devel/gettext-0.11
	>=dev-util/gtk-doc-am-1.15
	doc? (
		>=dev-libs/libxslt-1.0
		>=dev-util/gtk-doc-1.15
		~app-text/docbook-xml-dtd-4.1.2 )
	systemtap? ( >=dev-util/systemtap-1.3 )
	test? (
		>=dev-util/gdbus-codegen-2.30.0
		>=sys-apps/dbus-1.2.14 )
	!<dev-util/gtk-doc-1.15-r2"
PDEPEND="!<gnome-base/gvfs-1.6.4-r990" # Earlier versions do not work with glib

# XXX: Consider adding test? ( sys-devel/gdb ); assert-msg-test tries to use it

src_prepare() {
	mv -vf "${WORKDIR}"/pkg-config-*/pkg.m4 "${WORKDIR}"/ || die

	if use ia64 ; then
		# Only apply for < 4.1
		local major=$(gcc-major-version)
		local minor=$(gcc-minor-version)
		if (( major < 4 || ( major == 4 && minor == 0 ) )); then
			epatch "${FILESDIR}/glib-2.10.3-ia64-atomic-ops.patch"
		fi
	fi

	# Don't fail gio tests when ran without userpriv, upstream bug 552912
	# This is only a temporary workaround, remove as soon as possible
	epatch "${FILESDIR}/${PN}-2.18.1-workaround-gio-test-failure-without-userpriv.patch"

	# Fix gmodule issues on fbsd; bug #184301
	epatch "${FILESDIR}"/${PN}-2.12.12-fbsd.patch

	# Fix test failure when upgrading from 2.22 to 2.24, upstream bug 621368
	epatch "${FILESDIR}/${PN}-2.24-assert-test-failure.patch"

	# Do not try to remove files on live filesystem, upstream bug #619274
	sed 's:^\(.*"/desktop-app-info/delete".*\):/*\1*/:' \
		-i "${S}"/gio/tests/desktop-app-info.c || die "sed failed"

	if ! use test; then
		# don't waste time building tests
		sed 's/^\(.*\SUBDIRS .*\=.*\)tests\(.*\)$/\1\2/' -i $(find . -name Makefile.am -o -name Makefile.in) || die
	else
		# Disable tests requiring dev-util/desktop-file-utils when not installed, bug #286629
		if ! has_version dev-util/desktop-file-utils ; then
			ewarn "Some tests will be skipped due dev-util/desktop-file-utils not being present on your system,"
			ewarn "think on installing it to get these tests run."
			sed -i -e "/appinfo\/associations/d" gio/tests/appinfo.c || die
			sed -i -e "/desktop-app-info\/default/d" gio/tests/desktop-app-info.c || die
			sed -i -e "/desktop-app-info\/fallback/d" gio/tests/desktop-app-info.c || die
			sed -i -e "/desktop-app-info\/lastused/d" gio/tests/desktop-app-info.c || die
		fi
	fi

	# gdbus-codegen is a separate package
	epatch "${FILESDIR}/${PN}-2.29.18-external-gdbus-codegen.patch"

	# Skip the qsort_r test, which fails to run under cross-compilation,
	# and thus use the qsort_r implementation provided by glib
	epatch "${FILESDIR}/${PN}-2.30.0-qsort_r-check.patch"

	# Add the missing G_BEGIN_DECLS / G_END_DECLS in header files.
	# upstream bug 663880
	epatch "${FILESDIR}/${PN}-2.30.0-missing-decls.patch"

	# disable pyc compiling
	ln -sfn $(type -P true) py-compile

	# Needed for the punt-python-check patch, disabling timeout test
	# Also needed to prevent croscompile failures, see bug #267603
	# Also needed for the no-gdbus-codegen patch
	AT_M4DIR="${WORKDIR}" eautoreconf

	[[ ${CHOST} == *-freebsd* ]] && elibtoolize

	epunt_cxx
}

src_configure() {
	# Avoid circular depend with dev-util/pkgconfig
	if ! has_version dev-util/pkgconfig; then
		export DBUS1_CFLAGS="-I/usr/include/dbus-1.0 -I/usr/$(get_libdir)/dbus-1.0/include"
		export DBUS1_LIBS="-ldbus-1"
	fi

	local myconf

	# Building with --disable-debug highly unrecommended.  It will build glib in
	# an unusable form as it disables some commonly used API.  Please do not
	# convert this to the use_enable form, as it results in a broken build.
	# -- compnerd (3/27/06)
	use debug && myconf="--enable-debug"

	# Always use internal libpcre, bug #254659
	econf ${myconf} \
		$(use_enable xattr) \
		$(use_enable doc man) \
		$(use_enable doc gtk-doc) \
		$(use_enable fam) \
		$(use_enable selinux) \
		$(use_enable static-libs static) \
		$(use_enable systemtap dtrace) \
		$(use_enable systemtap systemtap) \
		--enable-regex \
		--with-pcre=internal \
		--with-threads=posix
}

src_install() {
	local f
	emake DESTDIR="${D}" install || die "Installation failed"

	# Do not install charset.alias even if generated, leave it to libiconv
	rm -f "${ED}/usr/lib/charset.alias"

	# Don't install gdb python macros, bug 291328
	rm -rf "${ED}/usr/share/gdb/" "${ED}/usr/share/glib-2.0/gdb/"

	dodoc AUTHORS ChangeLog* NEWS* README || die "dodoc failed"

	insinto /usr/share/bash-completion
	for f in gdbus gsettings; do
		newins "${ED}/etc/bash_completion.d/${f}-bash-completion.sh" ${f} || die
	done
	rm -rf "${ED}/etc"

	# Completely useless with or without USE static-libs, people need to use
	# pkg-config
	find "${ED}" -name '*.la' -exec rm -f {} +
}

src_test() {
	unset DBUS_SESSION_BUS_ADDRESS
	export XDG_CONFIG_DIRS=/etc/xdg
	export XDG_DATA_DIRS=/usr/local/share:/usr/share
	export G_DBUS_COOKIE_SHA1_KEYRING_DIR="${T}/temp"
	export XDG_DATA_HOME="${T}"
	unset GSETTINGS_BACKEND # bug 352451

	# Related test is a bit nitpicking
	mkdir "$G_DBUS_COOKIE_SHA1_KEYRING_DIR"
	chmod 0700 "$G_DBUS_COOKIE_SHA1_KEYRING_DIR"

	# Hardened: gdb needs this, bug #338891
	if host-is-pax ; then
		pax-mark -mr "${S}"/tests/.libs/assert-msg-test \
			|| die "Hardened adjustment failed"
	fi

	# Need X for dbus-launch session X11 initialization
	Xemake check || die "tests failed"
}

pkg_preinst() {
	# Only give the introspection message if:
	# * The user has gobject-introspection
	# * Has glib already installed
	# * Previous version was different from new version
	if has_version "dev-libs/gobject-introspection" && ! has_version "=${CATEGORY}/${PF}"; then
		ewarn "You must rebuild gobject-introspection so that the installed"
		ewarn "typelibs and girs are regenerated for the new APIs in glib"
	fi
}

pkg_postinst() {
	# Inform users about possible breakage when updating glib and not dbus-glib, bug #297483
	if has_version dev-libs/dbus-glib; then
		ewarn "If you experience a breakage after updating dev-libs/glib try"
		ewarn "rebuilding dev-libs/dbus-glib"
	fi

	if has_version '<x11-libs/gtk+-3.0.12:3'; then
		# To have a clear upgrade path for gtk+-3.0.x users, have to resort to
		# a warning instead of a blocker
		ewarn
		ewarn "Using <gtk+-3.0.12:3 with ${P} results in frequent crashes."
		ewarn "You should upgrade to a newer version of gtk+:3 immediately."
	fi
}
