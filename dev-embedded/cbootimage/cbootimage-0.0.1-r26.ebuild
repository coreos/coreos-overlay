# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="594d7b583de7b49d93f0fb1553d46a43c6b6f8b2"
CROS_WORKON_TREE="3c1f68b1c84462e5e30c6f8f6f354ecff67f44bd"
CROS_WORKON_PROJECT="chromiumos/third_party/cbootimage"

inherit cros-workon

DESCRIPTION="Utility for signing Tegra2 boot images"
HOMEPAGE="http://git.chromium.org"
SRC_URI=""
LICENSE="GPLv2"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

RDEPEND=""
DEPEND=""

src_compile() {
	emake || die "emake failed"
}

src_install() {
	dodir /usr/bin
        exeinto /usr/bin

        doexe cbootimage
        doexe bct_dump
}
