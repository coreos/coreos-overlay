# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="2"

inherit eutils toolchain-funcs cros-workon

DESCRIPTION="GNU GRUB 2 boot loader"
HOMEPAGE="http://www.gnu.org/software/grub/"
LICENSE="GPL-3"
SLOT="0"
KEYWORDS="~amd64"
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
