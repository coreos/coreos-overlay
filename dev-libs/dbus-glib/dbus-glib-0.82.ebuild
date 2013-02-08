# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/dbus-glib/dbus-glib-0.82.ebuild,v 1.1 2009/11/07 20:00:54 eva Exp $

EAPI="2"

inherit eutils bash-completion

DESCRIPTION="D-Bus bindings for glib"
HOMEPAGE="http://dbus.freedesktop.org/"
SRC_URI="http://dbus.freedesktop.org/releases/${PN}/${P}.tar.gz"

LICENSE="|| ( GPL-2 AFL-2.1 )"
SLOT="0"
KEYWORDS="~alpha ~amd64 ~arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc ~x86 ~x86-fbsd"
IUSE="bash-completion debug doc test"

RDEPEND=">=sys-apps/dbus-1.1
	>=dev-libs/glib-2.10
	>=dev-libs/expat-1.95.8"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	sys-devel/gettext
	doc? (
		app-doc/doxygen
		app-text/xmlto
		>=dev-util/gtk-doc-1.4 )"

BASH_COMPLETION_NAME="dbus"

pkg_setup() {
	# We need a dbus-binding-tool installed on the host for cross-compilation.
	# So check if we have it in the path and can call it right at the beginning...
	if tc-is-cross-compiler ; then
		dbus-binding-tool --version >/dev/null 2>&1
		[[ "$?" != "0" ]] && die "Cross-compilation requires a dbus-binding-tool on the host. Please emerge dbus-glib to your host!"
	fi
}

src_prepare() {
	# description ?
	epatch "${FILESDIR}"/${PN}-introspection.patch
}

src_configure() {
	local myconf=""

	# For cross-compilation we need the host dbus-binding-tool
	if tc-is-cross-compiler ; then
		myconf="${myconf} \
				--with-dbus-binding-tool=dbus-binding-tool"
	fi

	econf \
		--localstatedir=/var \
		$(use_enable bash-completion) \
		$(use_enable debug verbose-mode) \
		$(use_enable debug checks) \
		$(use_enable debug asserts) \
		$(use_enable doc doxygen-docs) \
		$(use_enable doc gtk-doc) \
		$(use_enable test tests) \
		$(use_with test test-socket-dir "${T}"/dbus-test-socket) \
		${myconf}
}

src_install() {
	emake DESTDIR="${D}" install || die "make install failed"

	dodoc AUTHORS ChangeLog HACKING NEWS README || die "dodoc failed."

	# FIXME: We need --with-bash-completion-dir
	if use bash-completion ; then
		dobashcompletion "${D}"/etc/bash_completion.d/dbus-bash-completion.sh
		rm -rf "${D}"/etc/bash_completion.d || die "rm failed"
	fi
}
