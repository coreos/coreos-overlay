# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/third_party/tegra-power-query"

inherit cros-workon

DESCRIPTION="Utility monitoring power usage on Harmony and Seaboard"
HOMEPAGE=""
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm ~x86"
IUSE=""

RDEPEND=""
DEPEND=""

src_compile() {
	emake || die "emake failed"
}

src_install() {
	dodir /usr/bin
        exeinto /usr/bin

        doexe tegra-power-query || die "doexe failed"
}
