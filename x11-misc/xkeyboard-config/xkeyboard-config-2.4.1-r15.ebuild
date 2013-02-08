# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-misc/xkeyboard-config/xkeyboard-config-2.4.1-r3.ebuild,v 1.5 2012/01/24 12:52:59 jer Exp $

EAPI=4

XORG_STATIC=no
inherit xorg-2

EGIT_REPO_URI="git://anongit.freedesktop.org/git/xkeyboard-config"

DESCRIPTION="X keyboard configuration database"
HOMEPAGE="http://www.freedesktop.org/wiki/Software/XKeyboardConfig"
[[ ${PV} == *9999* ]] || SRC_URI="${XORG_BASE_INDIVIDUAL_URI}/data/${P}.tar.bz2"

KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~x86-fbsd ~x86-interix ~amd64-linux ~x86-linux ~ppc-macos ~x86-macos ~sparc-solaris ~x86-solaris"
IUSE="parrot"

LICENSE="MIT"
SLOT="0"

RDEPEND=">=x11-apps/xkbcomp-1.2.1
	>=x11-libs/libX11-1.4.2"
DEPEND="${RDEPEND}
	x11-proto/xproto
	>=dev-util/intltool-0.30
	dev-perl/XML-Parser"

XORG_CONFIGURE_OPTIONS=(
	--with-xkb-base="${EPREFIX}/usr/share/X11/xkb"
	--enable-compat-rules
	# do not check for runtime deps
	--disable-runtime-deps
	--with-xkb-rules-symlink=xorg
)

PATCHES=(
	"${FILESDIR}"/${P}-extended-function-keys.patch
	"${FILESDIR}"/xorg-cve-2012-0064.patch
	"${FILESDIR}"/${P}-gb-dvorak-deadkey.patch
	"${FILESDIR}"/${P}-no-keyboard.patch
	"${FILESDIR}"/${P}-colemack-neo-capslock-remap.patch
	"${FILESDIR}"/${P}-remap-capslock.patch
	"${FILESDIR}"/${P}-add-f19-24.patch
	"${FILESDIR}"/${P}-gb-extd-deadkey.patch
	"${FILESDIR}"/${P}-remap-f15-as-mod2mask.patch
)

use parrot && PATCHES+=( "${FILESDIR}"/${P}-parrot-euro-sign.patch )

src_prepare() {
	xorg-2_src_prepare
	if [[ ${XORG_EAUTORECONF} != no ]]; then
		intltoolize --copy --automake || die
	fi
}

src_compile() {
	# cleanup to make sure .dir files are regenerated
	# bug #328455 c#26
	xorg-2_src_compile clean
	xorg-2_src_compile
}
