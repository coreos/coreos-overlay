# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

CROS_WORKON_COMMIT="d1c858167c0caae45f8a07147c4e19f74edc7708"
CROS_WORKON_TREE="a626b63d9eb83c6985625569fa10d3b820ae9dd4"
inherit eutils toolchain-funcs cros-workon

DESCRIPTION="GNU GRUB 2 boot loader"
HOMEPAGE="http://www.gnu.org/software/grub/"
LICENSE="GPL-3"
SLOT="0"
KEYWORDS="amd64"
IUSE="truetype"
CROS_WORKON_PROJECT="chromiumos/third_party/grub2"

RDEPEND=">=sys-libs/ncurses-5.2-r5
	dev-libs/lzo
	truetype? ( media-libs/freetype )"
DEPEND="${RDEPEND}
	dev-lang/ruby"
PROVIDE="virtual/bootloader"

export STRIP_MASK="*/grub/*/*.mod"

CROS_WORKON_LOCALNAME="grub2"

src_configure() {
	econf \
		--disable-werror \
		--disable-grub-mkfont \
		--disable-grub-fstest \
		--disable-efiemu \
		--sbindir=/sbin \
		--bindir=/bin \
		--libdir=/$(get_libdir) \
		--with-platform=efi \
		--target=x86_64 \
		--program-prefix=
}

src_compile() {
	emake -j1 || die "${SRCPATH} compile failed."
}

src_install() {
	emake DESTDIR="${D}" install || die
}
