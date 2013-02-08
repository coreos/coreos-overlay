# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/nss/nss-3.12.8.ebuild,v 1.1 2010/09/30 11:58:39 anarchy Exp $

EAPI=3
inherit eutils flag-o-matic multilib toolchain-funcs

NSPR_VER="4.8.6"
RTM_NAME="NSS_${PV//./_}_RTM"
DESCRIPTION="Mozilla's Network Security Services library that implements PKI support"
HOMEPAGE="http://www.mozilla.org/projects/security/pki/nss/"
SRC_URI="ftp://ftp.mozilla.org/pub/mozilla.org/security/nss/releases/${RTM_NAME}/src/${P}.tar.gz"

LICENSE="|| ( MPL-1.1 GPL-2 LGPL-2.1 )"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~sparc x86 ~x86-fbsd ~amd64-linux ~x86-linux ~x86-macos ~sparc-solaris ~x64-solaris ~x86-solaris"

DEPEND="dev-util/pkgconfig"
RDEPEND=">=dev-libs/nspr-${NSPR_VER}
	>=dev-libs/nss-${PV}
	>=dev-db/sqlite-3.5
	sys-libs/zlib"

src_prepare() {
	# Custom changes for gentoo
	epatch "${FILESDIR}/${PN}-3.12.5-gentoo-fixups.diff"
	epatch "${FILESDIR}/${PN}-3.12.6-gentoo-fixup-warnings.patch"
	epatch "${FILESDIR}"/${P}-shlibsign.patch
	epatch "${FILESDIR}"/${P}-chromeos-root-certs.patch

	# See https://bugzilla.mozilla.org/show_bug.cgi?id=741481 for details.
	epatch "${FILESDIR}"/${P}-cert-initlocks.patch

	cd "${S}"/mozilla/security/coreconf

	# Explain that linux 3.0+ is just the same as 2.6.
	ln -sf Linux2.6.mk Linux$(uname -r | cut -b1-3).mk

	# hack nspr paths
	echo 'INCLUDES += -I'"${EPREFIX}"'/usr/include/nspr -I$(DIST)/include/dbm' \
		>> headers.mk || die "failed to append include"

	# modify install path
	sed -e 's:SOURCE_PREFIX = $(CORE_DEPTH)/\.\./dist:SOURCE_PREFIX = $(CORE_DEPTH)/dist:' \
		-i source.mk

	# Respect LDFLAGS
	sed -i -e 's/\$(MKSHLIB) -o/\$(MKSHLIB) \$(LDFLAGS) -o/g' rules.mk

	# Ensure we stay multilib aware
	sed -i -e "s:gentoo\/nss:$(get_libdir):" "${S}"/mozilla/security/nss/config/Makefile || die "Failed to fix for multilib"

	# Fix pkgconfig file for Prefix
	sed -i -e "/^PREFIX =/s:= /usr:= ${EPREFIX}/usr:" \
		"${S}"/mozilla/security/nss/config/Makefile

	epatch "${FILESDIR}"/${PN}-3.12.4-solaris-gcc.patch  # breaks non-gnu tools
	# dirty hack
	cd "${S}"/mozilla/security/nss
	sed -i -e "/CRYPTOLIB/s:\$(SOFTOKEN_LIB_DIR):../freebl/\$(OBJDIR):" \
		lib/ssl/config.mk || die
	sed -i -e "/CRYPTOLIB/s:\$(SOFTOKEN_LIB_DIR):../../lib/freebl/\$(OBJDIR):" \
		cmd/platlibs.mk || die
}

src_compile() {
	strip-flags

	echo > "${T}"/test.c
	$(tc-getCC) ${CFLAGS} -c "${T}"/test.c -o "${T}"/test.o
	case $(file "${T}"/test.o) in
	*64-bit*|*ppc64*|*x86_64*) export USE_64=1;;
	*32-bit*|*ppc*|*i386*) ;;
	*) die "Failed to detect whether your arch is 64bits or 32bits, disable distcc if you're using it, please";;
	esac

	export NSPR_INCLUDE_DIR="${ROOT}"/usr/include/nspr
	export NSPR_LIB_DIR="${ROOT}"/usr/lib
	export BUILD_OPT=1
	export NSS_USE_SYSTEM_SQLITE=1
	export NSDISTMODE=copy
	export NSS_ENABLE_ECC=1
	export XCFLAGS="${CFLAGS}"
	export FREEBL_NO_DEPEND=1

	# Cross-compile Love
	( filter-flags -m* ;
	  cd "${S}"/mozilla/security/coreconf &&
	  emake -j1 BUILD_OPT=1 XCFLAGS="${CFLAGS}" LDFLAGS= CC="$(tc-getBUILD_CC)" || die "coreconf make failed" )
	cd "${S}"/mozilla/security/dbm
	NSINSTALL=$(readlink -f $(find "${S}"/mozilla/security/coreconf -type f -name nsinstall))
	emake -j1 BUILD_OPT=1 XCFLAGS="${CFLAGS}" CC="$(tc-getCC)" NSINSTALL="${NSINSTALL}" OS_TEST=${ARCH} || die "dbm make failed"
	cd "${S}"/mozilla/security/nss
	if tc-is-cross-compiler; then
		SHLIBSIGN_ARG="SHLIBSIGN=/usr/bin/nssshlibsign"
	fi
	emake -j1 BUILD_OPT=1 XCFLAGS="${CFLAGS}" CC="$(tc-getCC)" NSINSTALL="${NSINSTALL}" OS_TEST=${ARCH} ${SHLIBSIGN_ARG} || die "nss make failed"
}

src_install () {
	local nssutils
	# The tests we do not need to install.
	#nssutils_test="bltest crmftest dbtest dertimetest
	#fipstest remtest sdrtest"
	nssutils="addbuiltin atob baddbdir btoa certcgi certutil checkcert
	cmsutil conflict crlutil derdump digest makepqg mangle modutil multinit
	nonspr10 ocspclnt oidcalc p7content p7env p7sign p7verify pk11mode
	pk12util pp rsaperf selfserv shlibsign signtool signver ssltap strsclnt
	symkeyutil tstclnt vfychain vfyserv"

	cd "${S}"/mozilla/security/dist/*/bin/
	for f in $nssutils; do
		# TODO(cmasone): switch to normal nss tool names
		newbin ${f} nss${f}
	done
}
