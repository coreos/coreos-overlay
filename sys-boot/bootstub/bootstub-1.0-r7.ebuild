# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

CROS_WORKON_COMMIT="5ac54e8d3d305c2c6c7297e8e54d3cf7e4629b29"
CROS_WORKON_TREE="a04ce1e804accdabc6afee4a4ca5ecd9e64d6c72"
inherit eutils toolchain-funcs cros-workon

DESCRIPTION="Chrome OS embedded bootstub"
LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64"
IUSE=""
CROS_WORKON_PROJECT="chromiumos/third_party/bootstub"
DEPEND="sys-boot/gnu-efi"

src_compile() {
	emake -j1 CC="$(tc-getCC)" LD="$(tc-getLD)" \
              OBJCOPY="$(tc-getPROG OBJCOPY objcopy)" \
              || die "${SRCPATH} compile failed."
}

src_install() {
	LIBDIR=$(get_libdir)
	emake DESTDIR="${D}/${LIBDIR}/bootstub" install || \
              die "${SRCPATH} install failed."
}
