# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
CROS_WORKON_COMMIT="52a5bdaa8930c14ee42518354de3e5ec09911c6b"
CROS_WORKON_TREE="a214d22b4c8a8f3cf46dd4a8b80451231eedb9dc"
CROS_WORKON_PROJECT="chromiumos/third_party/atheros"

inherit cros-workon

DESCRIPTION="Atheros AR300x firmware"
HOMEPAGE="http://www.atheros.com/"
LICENSE="Atheros"

SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RESTRICT="binchecks strip test"
CROS_WORKON_LOCALNAME="atheros"
DEPEND=""
RDEPEND=""

src_install() {
    src_dir="${S}"/ath3k/files/firmware
    dodir /lib/firmware || die
    insinto /lib/firmware
    doins -r ${src_dir}/* || die \
    	  "failed installing from ${src_dir} to ${D}/lib/firmware"
}
