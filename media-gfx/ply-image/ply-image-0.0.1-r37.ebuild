# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI=2
CROS_WORKON_COMMIT="ef0350689d0cc3bf2c845c098adf6efb666cbbec"
CROS_WORKON_TREE="b3354be712b630953df065a934a4063b12fa2843"
CROS_WORKON_PROJECT="chromiumos/third_party/ply-image"

inherit toolchain-funcs cros-workon

DESCRIPTION="Utility that dumps a png image to the frame buffer."
HOMEPAGE="http://www.chromium.org/"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 x86 arm"
IUSE=""

DEPEND="media-libs/libpng
	x11-libs/libdrm"
RDEPEND="${DEPEND}"

src_compile() {
	if tc-is-cross-compiler ; then
		tc-getCC
	fi
	emake || die "emake failed"
}

src_install() {
	mkdir -p "${D}/usr/bin"
	cp "${S}/src/ply-image" "${D}/usr/bin"
}
