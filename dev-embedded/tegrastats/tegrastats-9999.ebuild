# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_PROJECT="chromiumos/third_party/tegrastats"

DESCRIPTION="Software to inspect and adjust DVFS parameters on tegra."
HOMEPAGE=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="~amd64 ~arm"
IUSE=""

inherit cros-workon toolchain-funcs

RDEPEND=""
DEPEND=""

src_compile() {
	tc-export CC CXX AR RANLIB LD NM PKG_CONFIG
	emake || die "emake failed"
}

src_install() {
	dodir /usr/bin
	exeinto /usr/bin
	doexe tegrastats/tegrastats || die "doexe failed"
	doexe dfs_stress/dfs_stress || die "doexe failed"
}
