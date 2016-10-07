# Copyright 1999-2016 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI="5"

inherit eutils toolchain-funcs flag-o-matic

code_ver=${PV}
data_ver=${PV}
DESCRIPTION="Timezone data (/usr/share/zoneinfo) and utilities (tzselect/zic/zdump)"
HOMEPAGE="http://www.iana.org/time-zones http://www.twinsun.com/tz/tz-link.htm"
SRC_URI="http://www.iana.org/time-zones/repository/releases/tzdata${data_ver}.tar.gz
	http://www.iana.org/time-zones/repository/releases/tzcode${code_ver}.tar.gz"

LICENSE="BSD public-domain"
SLOT="0"
KEYWORDS="alpha amd64 arm arm64 hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd ~x86-freebsd ~amd64-linux ~ia64-linux ~x86-linux ~ppc-macos ~x64-macos ~x86-macos"
IUSE="nls leaps_timezone elibc_FreeBSD"

DEPEND="nls? ( virtual/libintl )"
RDEPEND="${DEPEND}
	!sys-libs/glibc[vanilla(+)]"

S=${WORKDIR}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-2016c-makefile.patch
	tc-is-cross-compiler && cp -pR "${S}" "${S}"-native
}

src_configure() {
	tc-export CC

	append-lfs-flags #471102

	if use elibc_FreeBSD || use elibc_Darwin ; then
		append-cppflags -DSTD_INSPIRED #138251
	fi

	append-cppflags -DHAVE_GETTEXT=$(usex nls 1 0) -DTZ_DOMAIN='\"libc\"'
	LDLIBS=""
	if use nls ; then
		# See if an external libintl is available. #154181 #578424
		local c="${T}/test"
		echo 'main(){}' > "${c}.c"
		if $(tc-getCC) ${CPPFLAGS} ${CFLAGS} ${LDFLAGS} "${c}.c" -o "${c}" -lintl 2>/dev/null ; then
			LDLIBS+=" -lintl"
		fi
	fi
}

_emake() {
	emake \
		TOPDIR="${EPREFIX}/usr" \
		REDO=$(usex leaps_timezone posix_right posix_only) \
		"$@"
}

src_compile() {
	# TOPDIR is used in some utils when compiling.
	_emake \
		AR="$(tc-getAR)" \
		CC="$(tc-getCC)" \
		RANLIB="$(tc-getRANLIB)" \
		CFLAGS="${CFLAGS} -std=gnu99" \
		LDFLAGS="${LDFLAGS}" \
		LDLIBS="${LDLIBS}"
	if tc-is-cross-compiler ; then
		_emake -C "${S}"-native \
			CC="$(tc-getBUILD_CC)" \
			CFLAGS="${BUILD_CFLAGS}" \
			CPPFLAGS="${BUILD_CPPFLAGS}" \
			LDFLAGS="${BUILD_LDFLAGS}" \
			LDLIBS="${LDLIBS}" \
			zic
	fi
}

src_install() {
	local zic=""
	tc-is-cross-compiler && zic="zic=${S}-native/zic"
	_emake install ${zic} DESTDIR="${D}"
	dodoc CONTRIBUTING README NEWS Theory
	dohtml *.htm

	# install the symlink by hand to not break existing timezones
	dosym . /usr/share/zoneinfo/posix
}
