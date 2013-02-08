# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="5a70d590af8ae7211ab0a3ce69433cb3d9ffa144"
CROS_WORKON_TREE="4614c03641dd88053b2bab281ab385ddfac73551"
CROS_WORKON_PROJECT="chromiumos/third_party/cypress-tools"
inherit toolchain-funcs cros-workon

DESCRIPTION="Cypress APA Trackpad Firmware Update Utility"
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE=""

RDEPEND=""
DEPEND=""

CROS_WORKON_LOCALNAME=../third_party/cypress-tools
CROS_WORKON_SUBDIR=

src_compile() {
	tc-export CC
	emake || die "Compile failed"
}

src_install() {
	into /
	dosbin cyapa_fw_update

	insinto /opt/google/touchpad/cyapa
	doins images/CYTRA*
}
