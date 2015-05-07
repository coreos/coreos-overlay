# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-lang/spidermonkey/spidermonkey-17.0.0-r3.ebuild,v 1.7 2014/04/19 17:43:30 ago Exp $

EAPI="5"
WANT_AUTOCONF="2.1"
PYTHON_COMPAT=( python2_{6,7} )
PYTHON_REQ_USE="threads"
inherit eutils toolchain-funcs multilib python-any-r1 versionator pax-utils

MY_PN="mozjs"
MY_P="${MY_PN}${PV}"
DESCRIPTION="Stand-alone JavaScript C library"
HOMEPAGE="http://www.mozilla.org/js/spidermonkey/"
SRC_URI="http://ftp.mozilla.org/pub/mozilla.org/js/${MY_PN}${PV}.tar.gz"

LICENSE="NPL-1.1"
SLOT="17"
# "MIPS, MacroAssembler is not supported" wrt #491294 for -mips
KEYWORDS="alpha amd64 arm -hppa ia64 -mips ppc ppc64 ~s390 ~sh sparc x86 ~x86-fbsd"
IUSE="debug jit minimal static-libs test"

REQUIRED_USE="debug? ( jit )"
RESTRICT="ia64? ( test )"

S="${WORKDIR}/${MY_P}"
BUILDDIR="${S}/js/src"

RDEPEND=">=dev-libs/nspr-4.9.4
	virtual/libffi
	>=sys-libs/zlib-1.1.4"
DEPEND="${RDEPEND}
	${PYTHON_DEPS}
	app-arch/zip
	virtual/pkgconfig"

pkg_setup(){
	if [[ ${MERGE_TYPE} != "binary" ]]; then
		python-any-r1_pkg_setup
		export LC_ALL="C"
	fi
}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-${SLOT}-js-config-shebang.patch
	epatch "${FILESDIR}"/${PN}-${SLOT}-ia64-mmap.patch
	epatch "${FILESDIR}"/${PN}-17.0.0-fix-file-permissions.patch
	# Remove obsolete jsuword bug #506160
	sed -i -e '/jsuword/d' "${BUILDDIR}"/jsval.h ||die "sed failed"
	epatch_user

	if [[ ${CHOST} == *-freebsd* ]]; then
		# Don't try to be smart, this does not work in cross-compile anyway
		ln -sfn "${BUILDDIR}/config/Linux_All.mk" "${S}/config/$(uname -s)$(uname -r).mk" || die
	fi
}

src_configure() {
	local nspr_cflags nspr_libs
	cd "${BUILDDIR}" || die

	# Mozilla screws up the meaning of BUILD, HOST, and TARGET :(
	tc-export_build_env CC CXX LD AR RANLIB PKG_CONFIG \
		BUILD_CC BUILD_CXX BUILD_LD BUILD_AR BUILD_RANLIB
	export HOST_CC="${BUILD_CC}" HOST_CFLAGS="${BUILD_CFLAGS}" \
		   HOST_CXX="${BUILD_CXX}" HOST_CXXFLAGS="${BUILD_CXXFLAGS}" \
		   HOST_LD="${BUILD_LD}" HOST_LDFLAGS="${BUILD_LDFLAGS}" \
		   HOST_AR="${BUILD_AR}" HOST_RANLIB="${BUILD_RANLIB}"

	# Use pkg-config instead of nspr-config to use $SYSROOT
	nspr_cflags="$(${PKG_CONFIG} --cflags nspr)" || die
	nspr_libs="$(${PKG_CONFIG} --libs nspr)" || die

	econf \
		${myopts} \
		--host="${CBUILD}" \
		--target="${CHOST}" \
		--enable-jemalloc \
		--enable-readline \
		--enable-threadsafe \
		--enable-system-ffi \
		--with-nspr-cflags="${nspr_cflags}" \
		--with-nspr-libs="${nspr_libs}" \
		$(use_enable debug) \
		$(use_enable jit tracejit) \
		$(use_enable jit methodjit) \
		$(use_enable static-libs static) \
		$(use_enable test tests)
}

src_compile() {
	cd "${BUILDDIR}" || die
	emake
}

src_test() {
	cd "${BUILDDIR}/jsapi-tests" || die
	emake check
}

src_install() {
	cd "${BUILDDIR}" || die
	emake DESTDIR="${D}" install

	if ! use minimal; then
		if use jit; then
			pax-mark m "${ED}/usr/bin/js${SLOT}"
		fi
	else
		rm -f "${ED}/usr/bin/js${SLOT}"
	fi

	if ! use static-libs; then
		# We can't actually disable building of static libraries
		# They're used by the tests and in a few other places
		find "${D}" -iname '*.a' -delete || die
	fi
}
