# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/glu/glu-9.0.0.ebuild,v 1.2 2012/09/20 11:57:23 chithanh Exp $

EAPI=4

EGIT_REPO_URI="git://anongit.freedesktop.org/mesa/glu"

if [[ ${PV} = 9999* ]]; then
	GIT_ECLASS="git-2"
	EXPERIMENTAL="true"
fi

inherit autotools-utils multilib ${GIT_ECLASS}

DESCRIPTION="The OpenGL Utility Library"
HOMEPAGE="http://cgit.freedesktop.org/mesa/glu/"

if [[ ${PV} = 9999* ]]; then
	SRC_URI=""
else
	SRC_URI="ftp://ftp.freedesktop.org/pub/mesa/${PN}/${P}.tar.bz2"
fi

LICENSE="SGI-B-2.0"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~sh ~sparc x86 ~amd64-fbsd ~x86-fbsd ~x86-freebsd ~amd64-linux ~ia64-linux ~x86-linux ~sparc-solaris ~x64-solaris ~x86-solaris"
IUSE="multilib static-libs"

DEPEND="virtual/opengl"
RDEPEND="${DEPEND}
	multilib? ( !app-emulation/emul-linux-x86-opengl )"

foreachabi() {
	if use multilib; then
		local ABI
		for ABI in $(get_all_abis); do
			multilib_toolchain_setup ${ABI}
			AUTOTOOLS_BUILD_DIR=${WORKDIR}/${ABI} "${@}"
		done
	else
		"${@}"
	fi
}

src_unpack() {
	default
	[[ $PV = 9999* ]] && git-2_src_unpack
}

src_prepare() {
	AUTOTOOLS_AUTORECONF=1 autotools-utils_src_prepare
}

src_configure() {
	foreachabi autotools-utils_src_configure
}

src_compile() {
	foreachabi autotools-utils_src_compile
}

src_install() {
	foreachabi autotools-utils_src_install
}

src_test() {
	:;
}
