# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-apps/mesa-progs/mesa-progs-7.5.1.ebuild,v 1.8 2009/12/16 18:02:52 scarabeus Exp $

inherit eutils toolchain-funcs

MY_PN="${PN/m/M}"
MY_PN="${MY_PN/-progs}"
MY_P="${MY_PN}-${PV/_/-}"
LIB_P="${MY_PN}Lib-${PV/_/-}"
PROG_P="${MY_PN}Demos-${PV/_/-}"
DESCRIPTION="Mesa's OpenGL utility and demo programs (glxgears and glxinfo)"
HOMEPAGE="http://mesa3d.sourceforge.net/"
if [[ $PV = *_rc* ]]; then
	SRC_URI="http://www.mesa3d.org/beta/${LIB_P}.tar.gz
		http://www.mesa3d.org/beta/${PROG_P}.tar.gz"
elif [[ $PV = 9999 ]]; then
	SRC_URI=""
else
	SRC_URI="ftp://ftp.freedesktop.org/pub/mesa/${PV}/${LIB_P}.tar.bz2
		ftp://ftp.freedesktop.org/pub/mesa/${PV}/${PROG_P}.tar.bz2"
fi
LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ppc64 sh sparc x86 ~x86-fbsd"
IUSE=""

RDEPEND="virtual/glut
	virtual/opengl
	virtual/glu"

DEPEND="${RDEPEND}"

S="${WORKDIR}/${MY_P}"

pkg_setup() {
	if [[ ${KERNEL} == "FreeBSD" ]]; then
		CONFIG="freebsd"
	elif use x86; then
		CONFIG="linux-dri-x86"
	elif use amd64; then
		CONFIG="linux-dri-x86-64"
	elif use ppc; then
		CONFIG="linux-dri-ppc"
	else
		CONFIG="linux-dri"
	fi
}

src_unpack() {
	HOSTCONF="${S}/configs/${CONFIG}"

	unpack ${A}
	cd "${S}"

	# Kill this; we don't want /usr/X11R6/lib ever to be searched in this
	# build.
	echo "EXTRA_LIB_PATH =" >> ${HOSTCONF}

	echo "OPT_FLAGS = ${CFLAGS}" >> ${HOSTCONF}
	echo "CC = $(tc-getCC)" >> ${HOSTCONF}
	echo "CXX = $(tc-getCXX)" >> ${HOSTCONF}
	echo "LDFLAGS = ${LDFLAGS}" >> ${HOSTCONF}

	# Just executables here, no need to compile with -fPIC
	echo "PIC_FLAGS =" >> ${HOSTCONF}

	epatch "${FILESDIR}"/${P}-gold.patch
}

src_compile() {
	cd "${S}"/configs
	ln -s ${CONFIG} current

	cd "${S}"/progs/xdemos

	emake glxinfo || die "glxinfo failed"
	emake glxgears || die "glxgears failed"
}

src_install() {
	dobin "${S}"/progs/xdemos/{glxgears,glxinfo} || die
}
