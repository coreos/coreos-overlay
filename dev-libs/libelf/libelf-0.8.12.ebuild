# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/libelf/libelf-0.8.12.ebuild,v 1.1 2009/11/21 09:24:32 patrick Exp $

EAPI=2

inherit autotools eutils multilib

DESCRIPTION="A ELF object file access library"
HOMEPAGE="http://www.mr511.de/software/"
SRC_URI="http://www.mr511.de/software/${P}.tar.gz"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="arm ~alpha amd64 ~hppa ~ppc ~sparc x86 ~sparc-fbsd ~x86-fbsd"
IUSE="debug nls elibc_FreeBSD"

DEPEND="!dev-libs/elfutils
	nls? ( sys-devel/gettext )"
RDEPEND="${DEPEND}"

src_unpack() {
	unpack ${A}
	cd "${S}"

	if use elibc_FreeBSD; then
		# Stop libelf from stamping on the system nlist.h
		sed -i -e 's:nlist.h::g' lib/Makefile.in || die

		# Enable shared libs
		sed -i \
			-e 's:\*-linux\*\|\*-gnu\*:\*-linux\*\|\*-gnu\*\|\*-freebsd\*:' \
			configure || die
	fi
}

src_prepare() {
	eautoreconf
}

src_configure() {
	econf \
		$(use_enable nls) \
		$(use_enable debug) \
		--enable-shared \
		|| die "econf failed"
}

src_compile() {
	emake || die "emake failed"
}

src_install() {
	emake -j1 \
		prefix="${D}"/usr \
		libdir="${D}"usr/$(get_libdir) \
		install \
		install-compat || die "emake install failed"
	dodoc ChangeLog VERSION README
}
