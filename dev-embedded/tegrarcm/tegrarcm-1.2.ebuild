# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=4

EGIT_REPO_URI="git://nv-tegra.nvidia.com/tools/tegrarcm.git"
EGIT_COMMIT="v${PV}"
inherit git-2

inherit autotools

DESCRIPTION="Utility for downloading code to tegra system in recovery mode"
HOMEPAGE="http://sourceforge.net/projects/tegra-rcm/"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86"
IUSE=""

RDEPEND=">=dev-libs/crypto++-5.6
	virtual/libusb:1"
DEPEND="${RDEPEND}"

src_prepare() {
	eautoreconf
}

src_install() {
	dobin src/tegrarcm
}
