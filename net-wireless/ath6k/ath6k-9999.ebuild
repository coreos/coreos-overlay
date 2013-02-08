# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
CROS_WORKON_PROJECT="chromiumos/third_party/atheros"

inherit cros-workon

DESCRIPTION="Atheros AR600x firmware"
HOMEPAGE="http://www.atheros.com/"
LICENSE="Atheros"

SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

RESTRICT="binchecks strip test"
CROS_WORKON_LOCALNAME="atheros"
DEPEND=""
RDEPEND=""

src_install() {
    src_dir="${S}"/ath6k/files/firmware
    dodir /lib/firmware || die
    insinto /lib/firmware
    doins -r ${src_dir}/* || die \
    	  "failed installing from ${src_dir} to ${D}/lib/firmware"
}
