# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-libs/libXt/libXt-1.0.6.ebuild,v 1.6 2009/10/11 11:04:55 nixnut Exp $

# Must be before x-modular eclass is inherited
SNAPSHOT="yes"

inherit x-modular flag-o-matic

DESCRIPTION="X.Org Xt library"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ppc ~ppc64 ~s390 ~sh ~sparc x86 ~x86-fbsd"
IUSE=""

RDEPEND="x11-libs/libX11
	x11-libs/libSM
	x11-libs/libICE
	x11-proto/xproto
	x11-proto/kbproto"
DEPEND="${RDEPEND}"

# patch is in git master and macros are only needed if SNAPSHOT is set to "yes"
DEPEND="${DEPEND} >=x11-misc/util-macros-1.2"
PATCHES=("${FILESDIR}/libXt-1.0.6-cross.patch")

pkg_setup() {
	# No such function yet
	# x-modular_pkg_setup

	# (#125465) Broken with Bdirect support
	filter-flags -Wl,-Bdirect
	filter-ldflags -Bdirect
	filter-ldflags -Wl,-Bdirect
}

# msb: Copied from http://gentoo.mindzoo.de/index.cgi/changeset/151
# Reference: http://gentoo.mindzoo.de/index.cgi/ticket/3
x-modular_src_compile() {
	x-modular_src_configure

	# For some reason, CC gets cleared out after the x-modular_src_make
	# call so the recompile below would fail, so we save off the target CC.
	TARGET_CC=$(tc-getCC)

	# [Cross-Compile Love] Disable {C,LD}FLAGS and redefine CC= for makestr
	( filter-flags -m* ;
	  cd util &&
	    make CC=$(tc-getBUILD_CC) CFLAGS="${CFLAGS}" LDFLAGS= clean all )
	x-modular_src_make

	# [Cross-Compile Love] Recompile 'makestr' with the target compiler,
	# just in case we want to install it.
	( cd util && make CC="${TARGET_CC}" clean all )
}
