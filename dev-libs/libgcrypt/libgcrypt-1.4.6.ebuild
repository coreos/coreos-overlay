# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/libgcrypt/libgcrypt-1.4.6.ebuild,v 1.9 2010/10/16 15:16:18 jer Exp $

EAPI="2"

inherit autotools eutils flag-o-matic toolchain-funcs

DESCRIPTION="General purpose crypto library based on the code used in GnuPG"
HOMEPAGE="http://www.gnupg.org/"
SRC_URI="mirror://gnupg/libgcrypt/${P}.tar.bz2
	ftp://ftp.gnupg.org/gcrypt/${PN}/${P}.tar.bz2"

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~sparc-fbsd ~x86-fbsd"
IUSE=""

RDEPEND=">=dev-libs/libgpg-error-1.5"
DEPEND="${RDEPEND}"

src_prepare() {
	# Fix build failure with non-bash /bin/sh.
	eautoreconf

	epunt_cxx
}

src_configure() {
	# TODO: When cross compiling to x86, this package currently has an
	# issue where it does not define all needed symbols in libgcrypt.a
	# As soon as this is fixed then re-enable asm.
	if tc-is-cross-compiler ; then
		if [[ $(tc-arch) == x86 ]] ; then
			DISABLE_ASM="--disable-asm"
		fi
	fi

	if tc-is-cross-compiler ; then
		# The libgcrypt configure code is really stupid and assumes
		# that you have an underscore symbol whenever you cross-compile.
		# Ask gcc to see what it does instead.
		local symprefix=$($(tc-getCPP) -E - <<<__USER_LABEL_PREFIX__ | tail -1)
		case ${symprefix} in
		__USER_LABEL_PREFIX__) ;; # nothing we can do here
		"") export ac_cv_sys_symbol_underscore=no ;;
		_)  export ac_cv_sys_symbol_underscore=yes ;;
		*)  die "unknown symbol prefix: ${symprefix}" ;;
		esac
	fi

	# --disable-padlock-support for bug #201917
	# --with-gpg-error-prefix for http://bugs.gentoo.org/267479
	econf \
		$DISABLE_ASM \
		--disable-padlock-support \
		--disable-dependency-tracking \
		--with-pic \
		--enable-noexecstack \
		--with-gpg-error-prefix=${ROOT}/usr
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
	dodoc AUTHORS ChangeLog NEWS README* THANKS TODO
}
