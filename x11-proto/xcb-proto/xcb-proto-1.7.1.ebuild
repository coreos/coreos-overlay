# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-proto/xcb-proto/xcb-proto-1.6-r2.ebuild,v 1.5 2011/12/27 21:13:20 maekke Exp $

EAPI=3
PYTHON_DEPEND="2:2.5"

inherit python xorg-2

DESCRIPTION="X C-language Bindings protocol headers"
HOMEPAGE="http://xcb.freedesktop.org/"
EGIT_REPO_URI="git://anongit.freedesktop.org/git/xcb/proto"
[[ ${PV} != 9999* ]] && \
	SRC_URI="http://xcb.freedesktop.org/dist/${P}.tar.bz2"

KEYWORDS="~alpha amd64 arm hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~x86-fbsd ~x86-freebsd ~x86-interix ~amd64-linux ~x86-linux ~ppc-macos ~x64-macos ~x86-macos ~sparc-solaris ~sparc64-solaris ~x64-solaris ~x86-solaris"

RDEPEND=""
DEPEND="${RDEPEND}
	dev-libs/libxml2"

pkg_setup() {
	python_set_active_version 2
}

src_prepare() {
	xorg-2_src_prepare
}

src_install() {
	xorg-2_src_install
	python_clean_installation_image
}

pkg_postinst() {
	python_mod_optimize xcbgen
	ewarn "Please rebuild both libxcb and xcb-util if you are upgrading from version 1.6"
}

pkg_postrm() {
	python_mod_cleanup xcbgen
}
