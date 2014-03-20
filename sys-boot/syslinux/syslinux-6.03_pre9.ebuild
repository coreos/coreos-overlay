# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-boot/syslinux/syslinux-6.03_pre1.ebuild,v 1.2 2014/02/09 18:04:43 zerochaos Exp $

EAPI=5

inherit eutils toolchain-funcs

DESCRIPTION="SYSLINUX, PXELINUX, ISOLINUX, EXTLINUX and MEMDISK bootloaders"
HOMEPAGE="http://www.syslinux.org/"
SRC_URI="mirror://kernel/linux/utils/boot/syslinux/Testing/${PV:0:4}/${P/_/-}.tar.xz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="-* amd64 x86"
IUSE="custom-cflags +perl"

RDEPEND="sys-apps/util-linux
		sys-fs/mtools
		perl? (
			dev-lang/perl
			dev-perl/Crypt-PasswdMD5
			dev-perl/Digest-SHA1
		)"
DEPEND="${RDEPEND}
	dev-lang/nasm
	virtual/os-headers"

S=${WORKDIR}/${P/_/-}

QA_PREBUILT="usr/share/${PN}/*.c32"

src_prepare() {
	# Don't prestrip or override user LDFLAGS, bug #305783
	local SYSLINUX_MAKEFILES="extlinux/Makefile linux/Makefile mtools/Makefile \
		sample/Makefile utils/Makefile"
	sed -i ${SYSLINUX_MAKEFILES} -e '/^LDFLAGS/d' || die "sed failed"

	if use custom-cflags; then
		sed -i ${SYSLINUX_MAKEFILES} \
			-e 's|-g -Os||g' \
			-e 's|-Os||g' \
			-e 's|CFLAGS[[:space:]]\+=|CFLAGS +=|g' \
			|| die "sed custom-cflags failed"
	else
		QA_FLAGS_IGNORED="
			/sbin/extlinux
			/usr/bin/memdiskfind
			/usr/bin/gethostip
			/usr/bin/isohybrid
			/usr/bin/syslinux
			"
	fi
	case ${ARCH} in
		amd64)	loaderarch="efi64" ;;
		x86)	loaderarch="efi32" ;;
		*)	ewarn "Unsupported architecture, building installers only." ;;
	esac

	# Don't build/install scripts if perl is disabled
	if ! use perl; then
		sed -i utils/Makefile \
			-e 's/$(TARGETS)/$(C_TARGETS)/' \
			-e 's/$(ASIS)//' \
			|| die "sed remove perl failed"
		rm man/{lss16toppm.1,ppmtolss16.1,syslinux2ansi.1} || die
	fi
}

src_compile() {
	# build system abuses the LDFLAGS variable to pass arguments to ld
	unset LDFLAGS
	if [[ ! -z ${loaderarch} ]]; then
		emake CC=$(tc-getCC) LD=$(tc-getLD) ${loaderarch}
	fi
	emake CC=$(tc-getCC) LD=$(tc-getLD) ${loaderarch} installer
}

src_install() {
	# parallel install fails sometimes
	einfo "loaderarch=${loaderarch}"
	emake -j1 LD=$(tc-getLD) INSTALLROOT="${D}" MANDIR=/usr/share/man bios ${loaderarch} install
	dodoc README NEWS doc/*.txt
}
