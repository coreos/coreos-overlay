# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-boot/syslinux/syslinux-3.83.ebuild,v 1.3 2010/02/26 12:10:54 fauli Exp $

inherit eutils flag-o-matic toolchain-funcs

DESCRIPTION="SysLinux, IsoLinux and PXELinux bootloader"
HOMEPAGE="http://syslinux.zytor.com/"
SRC_URI="mirror://kernel/linux/utils/boot/syslinux/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="-* amd64 x86"
IUSE=""

RDEPEND="sys-fs/mtools
		dev-perl/Crypt-PasswdMD5
		dev-perl/Digest-SHA1"
DEPEND="${RDEPEND}
	dev-lang/nasm"

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${PN}-3.72-nopie.patch
	# Don't prestrip, makes portage angry
	epatch "${FILESDIR}"/${PN}-3.72-nostrip.patch

	# Don't try to build win32/syslinux.exe
	epatch "${FILESDIR}/"${P}-disable_win32.patch

	# Disable the text banner for quieter boot.
	epatch "${FILESDIR}/"${P}-disable_banner.patch

	# Disable the blinking cursor as early as possible.
	epatch "${FILESDIR}/"${P}-disable_cursor.patch

	# Don't compile w/ PIC
	epatch "${FILESDIR}/"${P}-nopic.patch

	# Make it work when the compiler is 64 bit by default.
	epatch "${FILESDIR}/"${P}-x86_64.patch

	rm -f gethostip #bug 137081
}

src_compile() {
	# By default, syslinux wants you to use pre-built binaries
	# and only compile part of the package. Since we want to rebuild
	# everything from scratch we need to remove the prebuilts or else
	# some things don't get built with standard make.
	emake spotless || die "make spotless failed"

	# The syslinux build can't tolerate "-Wl,-O*"
	export LDFLAGS=$(raw-ldflags)

	if [ "${ROOT}" != "/" ]; then
		tc-export CC CXX AR RANLIB LD NM
		# Force GNU ld if gold is installed
		if ${LD}.bfd --version > /dev/null 2>&1; then
			LD=${LD}.bfd
		fi
		emake CC="$CC" CXX="$CXX" AR="$AR" RANLIB="$RANLIB" LD="$LD" \
			NM="$NM" || die "make failed"
	else
		emake || die "make failed"
	fi

}

src_install() {
	emake INSTALLSUBDIRS=utils INSTALLROOT="${D}" MANDIR=/usr/share/man install || die
	dodoc README NEWS TODO doc/*
}
