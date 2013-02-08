# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="108ebbfac4d13d11e1940216434c368384ee0c0b"
CROS_WORKON_TREE="5e354d36805a844694bf48c110c287067494722e"
CROS_WORKON_PROJECT="chromiumos/platform/uboot-env"

inherit cros-workon

DESCRIPTION="Python script to read/write u-boot environment"
SLOT="0"
KEYWORDS="arm x86"
IUSE=""

DEPEND=">=dev-lang/python-2.5"
RDEPEND="${DEPEND}"

src_install() {
	dobin ${S}/uboot-env.py || die
}
