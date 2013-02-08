# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-fonts/ja-ipafonts/ja-ipafonts-003.01.ebuild,v 1.5 2009/10/07 20:54:14 maekke Exp $

inherit font

MY_P="IPAMGTTC00303_r1"
DESCRIPTION="Japanese TrueType fonts developed by IPA (Information-technology Promotion Agency, Japan)"
HOMEPAGE="http://ossipedia.ipa.go.jp/ipafont/"
SRC_URI="http://commondatastorage.googleapis.com/chromeos-localmirror/distfiles/${MY_P}.tar.gz"

LICENSE="IPAfont"
SLOT="0"
KEYWORDS="alpha amd64 arm ~hppa ~ia64 ppc ~ppc64 ~s390 ~sh x86 ~x86-fbsd"
IUSE=""

S="${WORKDIR}/${MY_P}"
FONT_SUFFIX="ttc"
FONT_S="${S}"

DOCS="Readme*.txt"

# Only installs fonts
RESTRICT="strip binchecks"

src_install() {
        # call src_install() in font.eclass.
	font_src_install
}
