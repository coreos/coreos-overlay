# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="2"
CROS_WORKON_COMMIT="93ce6f598da2aa68003d1ebb305f32ec88755344"
CROS_WORKON_TREE="e678d0d27c25cba5abe0581e078ede22b87ba6ab"
CROS_WORKON_PROJECT="chromiumos/third_party/marvell"

inherit eutils cros-workon

DESCRIPTION="Marvell SD8787 firmware image"
HOMEPAGE="http://www.marvell.com/"
LICENSE="Marvell International Ltd."

SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RESTRICT="binchecks strip test"

DEPEND=""
RDEPEND=""

CROS_WORKON_LOCALNAME="marvell"

src_install() {
    dodir /lib/firmware/mrvl || die
    cp -a "${S}"/sd87* "${D}"/lib/firmware/mrvl/ || die
}
