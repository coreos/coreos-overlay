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
	>=dev-db/sqlite-3.5
	sys-libs/zlib"

src_prepare() {
	# Custom changes for gentoo
	epatch "${FILESDIR}/${PN}-3.12.5-gentoo-fixups.diff"
	epatch "${FILESDIR}/${PN}-3.12.6-gentoo-fixup-warnings.patch"
	epatch "${FILESDIR}"/${P}-shlibsign.patch
	epatch "${FILESDIR}"/${P}-chromeos-root-certs.patch
	epatch "${FILESDIR}"/${P}-remove-fortezza.patch
	epatch "${FILESDIR}"/${P}-chromeos-cert-nicknames.patch
	epatch "${FILESDIR}"/${P}-chromeos-mitm-root.patch

	# See https://bugzilla.mozilla.org/show_bug.cgi?id=741481 for details.
	epatch "${FILESDIR}"/${P}-cert-initlocks.patch

	# See http://crosbug.com/29623 for details.
	epatch "${FILESDIR}"/${P}-abort-on-failed-urandom-access.patch

	# See https://bugzilla.mozilla.org/show_bug.cgi?id=802429 for details
	epatch "${FILESDIR}"/${P}-bugzilla-802429.patch

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

# Altering these 3 libraries breaks the CHK verification.
# All of the following cause it to break:
# - stripping
# - prelink
# - ELF signing
# http://www.mozilla.org/projects/security/pki/nss/tech-notes/tn6.html
# Either we have to NOT strip them, or we have to forcibly resign after
# stripping.
#local_libdir="$(get_libdir)"
#export STRIP_MASK="
#	*/${local_libdir}/libfreebl3.so*
#	*/${local_libdir}/libnssdbm3.so*
#	*/${local_libdir}/libsoftokn3.so*"

export NSS_CHK_SIGN_LIBS="freebl3 nssdbm3 softokn3"

generate_chk() {
	local shlibsign="$1"
	local libdir="$2"
	einfo "Resigning core NSS libraries for FIPS validation"
	shift 2
	for i in ${NSS_CHK_SIGN_LIBS} ; do
		local libname=lib${i}.so
		local chkname=lib${i}.chk
		"${shlibsign}" \
			-i "${libdir}"/${libname} \
			-o "${libdir}"/${chkname}.tmp \
		&& mv -f \
			"${libdir}"/${chkname}.tmp \
			"${libdir}"/${chkname} \
		|| die "Failed to sign ${libname}"
	done
}

cleanup_chk() {
	local libdir="$1"
	shift 1
	for i in ${NSS_CHK_SIGN_LIBS} ; do
		local libfname="${libdir}/lib${i}.so"
		# If the major version has changed, then we have old chk files.
		[ ! -f "${libfname}" -a -f "${libfname}.chk" ] \
			&& rm -f "${libfname}.chk"
	done
}

src_install () {
	MINOR_VERSION=12
	cd "${S}"/mozilla/security/dist

	dodir /usr/$(get_libdir)
	cp -L */lib/*$(get_libname) "${ED}"/usr/$(get_libdir) || die "copying shared libs failed"
	# We generate these after stripping the libraries, else they don't match.
	#cp -L */lib/*.chk "${ED}"/usr/$(get_libdir) || die "copying chk files failed"
	cp -L */lib/libcrmf.a "${ED}"/usr/$(get_libdir) || die "copying libs failed"

	# Install nss-config and pkgconfig file
	dodir /usr/bin
	cp -L */bin/nss-config "${ED}"/usr/bin
	dodir /usr/$(get_libdir)/pkgconfig
	cp -L */lib/pkgconfig/nss.pc "${ED}"/usr/$(get_libdir)/pkgconfig

	# all the include files
	insinto /usr/include/nss
	doins public/nss/*.h
	cd "${ED}"/usr/$(get_libdir)
	local n=
	for file in *$(get_libname); do
		n=${file%$(get_libname)}$(get_libname ${MINOR_VERSION})
		mv ${file} ${n}
		ln -s ${n} ${file}
		if [[ ${CHOST} == *-darwin* ]]; then
			install_name_tool -id "${EPREFIX}/usr/$(get_libdir)/${n}" ${n} || die
		fi
	done

	local nssutils
	if [ ! tc-is-cross-compiler ]; then
		# Unless cross-compiling, enabled because we need it for chk generation.
		nssutils="shlibsign"
	fi
	cd "${S}"/mozilla/security/dist/*/bin/
	for f in $nssutils; do
		# TODO(cmasone): switch to normal nss tool names
		newbin ${f} nss${f}
	done

	# Prelink breaks the CHK files. We don't have any reliable way to run
	# shlibsign after prelink.
	declare -a libs
	for l in ${NSS_CHK_SIGN_LIBS} ; do
		libs+=("${EPREFIX}/usr/$(get_libdir)/lib${l}.so")
	done
	OLD_IFS="${IFS}" IFS=":" ; liblist="${libs[*]}" ; IFS="${OLD_IFS}"
	echo -e "PRELINK_PATH_MASK=${liblist}" >"${T}/90nss"
	unset libs liblist
	doenvd "${T}/90nss"
}

pkg_postinst() {
	elog "We have reverted back to using upstreams soname."
	elog "Please run revdep-rebuild --library libnss3.so.12 , this"
	elog "will correct most issues. If you find a binary that does"
	elog "not run please re-emerge package to ensure it properly"
	elog " links after upgrade."
	elog
	local tool_root
	# We must re-sign the libraries AFTER they are stripped.
	if [ ! tc-is-cross-compiler ]; then
		tool_root = "${EROOT}"
	fi
	generate_chk "${tool_root}"/usr/bin/nssshlibsign "${EROOT}"/usr/$(get_libdir)
}

pkg_postrm() {
	cleanup_chk "${EROOT}"/usr/$(get_libdir)
}
