# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Id$

EAPI="5"
WANT_AUTOCONF="2.1"
PYTHON_COMPAT=( python2_7 )
PYTHON_REQ_USE="threads"
inherit autotools eutils toolchain-funcs multilib python-any-r1 versionator pax-utils

MY_PN="js"
TARBALL_PV="$(replace_all_version_separators '' $(get_version_component_range 1-3))"
MY_P="${MY_PN}-${PV}"
TARBALL_P="${MY_PN}${TARBALL_PV}-1.0.0"
DESCRIPTION="Stand-alone JavaScript C library"
HOMEPAGE="http://www.mozilla.org/js/spidermonkey/"
SRC_URI="https://ftp.mozilla.org/pub/mozilla.org/js/${TARBALL_P}.tar.gz"

LICENSE="NPL-1.1"
SLOT="0/mozjs185"
KEYWORDS="alpha amd64 arm arm64 hppa ia64 ~mips ppc ppc64 s390 sh sparc x86 ~amd64-fbsd ~x86-fbsd ~x64-macos"
IUSE="debug minimal static-libs test"

S="${WORKDIR}/${MY_P}"
BUILDDIR="${S}/js/src"

RDEPEND=">=dev-libs/nspr-4.7.0
	x64-macos? ( dev-libs/jemalloc )"
DEPEND="${RDEPEND}
	${PYTHON_DEPS}
	app-arch/zip
	virtual/pkgconfig"

pkg_setup(){
	if [[ ${MERGE_TYPE} != "binary" ]]; then
		export LC_ALL="C"
	fi
}

src_prepare() {
	# https://bugzilla.mozilla.org/show_bug.cgi?id=628723#c43
	epatch "${FILESDIR}/${P}-fix-install-symlinks.patch"
	# https://bugzilla.mozilla.org/show_bug.cgi?id=638056#c9
	epatch "${FILESDIR}/${P}-fix-ppc64.patch"
	# https://bugs.gentoo.org/show_bug.cgi?id=400727
	# https://bugs.gentoo.org/show_bug.cgi?id=420471
	epatch "${FILESDIR}/${P}-arm_respect_cflags-3.patch"
	# https://bugs.gentoo.org/show_bug.cgi?id=438746
	epatch "${FILESDIR}"/${PN}-1.8.7-freebsd-pthreads.patch
	# https://bugs.gentoo.org/show_bug.cgi?id=441928
	epatch "${FILESDIR}"/${PN}-1.8.5-perf_event-check.patch
	# https://bugs.gentoo.org/show_bug.cgi?id=439260
	epatch "${FILESDIR}"/${P}-symbol-versions.patch
	# https://bugs.gentoo.org/show_bug.cgi?id=441934
	epatch "${FILESDIR}"/${PN}-1.8.5-ia64-fix.patch
	epatch "${FILESDIR}"/${PN}-1.8.5-ia64-static-strings.patch
	# https://bugs.gentoo.org/show_bug.cgi?id=431560
	epatch "${FILESDIR}"/${PN}-1.8.5-isfinite.patch
	# https://bugs.gentoo.org/show_bug.cgi?id=552786
	epatch "${FILESDIR}"/${PN}-perl-defined-array-check.patch
	# fix builds for alternate $ROOT locations
	epatch "${FILESDIR}"/${P}-no-link-lib-deps.patch
	# Fix for CONFIG_ARM64_VA_BITS_48=y
	# https://bugzilla.redhat.com/show_bug.cgi?id=1242326
	epatch "${FILESDIR}/${P}-fix-arm64-va-48.patch"

	epatch_user

	cd "${BUILDDIR}" || die
	eautoconf
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
		--with-nspr-cflags="${nspr_cflags}" \
		--with-nspr-libs="${nspr_libs}" \
		$(use_enable debug) \
		$(use_enable static-libs static) \
		$(use_enable test tests)
}

src_compile() {
	cd "${BUILDDIR}" || die
	emake
}

src_test() {
	cd "${BUILDDIR}/jsapi-tests" || die
	# for bug 415791
	pax-mark mr jsapi-tests
	emake check
}

src_install() {
	cd "${BUILDDIR}" || die
	emake DESTDIR="${D}" install
	# bug 437520 , exclude js shell for small systems
	if ! use minimal ; then
		dobin shell/js
		pax-mark m "${ED}/usr/bin/js"
	fi
	dodoc ../../README
	dohtml README.html

	if ! use static-libs; then
		# We can't actually disable building of static libraries
		# They're used by the tests and in a few other places
		find "${D}" -iname '*.a' -delete || die
	fi
}
