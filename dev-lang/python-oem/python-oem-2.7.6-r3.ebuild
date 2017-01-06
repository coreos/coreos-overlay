# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-lang/python/python-2.7.6.ebuild,v 1.8 2014/03/20 14:13:55 jer Exp $

EAPI="4"
WANT_AUTOMAKE="none"
WANT_LIBTOOL="none"

inherit autotools eutils flag-o-matic multilib pax-utils python-utils-r1 toolchain-funcs multiprocessing

MY_P="Python-${PV}"
PATCHSET_REVISION="1"

DESCRIPTION="An interpreted, interactive, object-oriented programming language"
HOMEPAGE="http://www.python.org/"
SRC_URI="http://www.python.org/ftp/python/${PV}/${MY_P}.tar.xz
	mirror://gentoo/python-gentoo-patches-${PV}-${PATCHSET_REVISION}.tar.xz
	http://dev.gentoo.org/~floppym/python/python-gentoo-patches-${PV}-${PATCHSET_REVISION}.tar.xz"

LICENSE="PSF-2"
SLOT="2.7"
KEYWORDS="~alpha amd64 ~arm arm64 hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc ~x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd"
IUSE="hardened"

# Do not add a dependency on dev-lang/python to this ebuild.
# If you need to apply a patch which requires python for bootstrapping, please
# run the bootstrap code on your dev box and include the results in the
# patchset. See bug 447752.

RDEPEND=""
DEPEND="app-arch/bzip2
	>=sys-libs/zlib-1.1.3
	virtual/libintl
	virtual/pkgconfig
	>=sys-devel/autoconf-2.65
	!sys-devel/gcc[libffi]"

S="${WORKDIR}/${MY_P}"

src_prepare() {
	if tc-is-cross-compiler; then
		local EPATCH_EXCLUDE="*_regenerate_platform-specific_modules.patch"
	fi

	EPATCH_SUFFIX="patch" epatch "${WORKDIR}/patches"

	# Fix for cross-compiling.
	epatch "${FILESDIR}/python-2.7.5-nonfatal-compileall.patch"

	# Fix for linux_distribution()
	epatch "${FILESDIR}/python-2.7.6-add_os_release_support.patch"

	# Fix for arm64 builds
	epatch "${FILESDIR}/python-2.7-aarch64-fix.patch"

	sed -i -e "s:@@GENTOO_LIBDIR@@:$(get_libdir):g" \
		Lib/distutils/command/install.py \
		Lib/distutils/sysconfig.py \
		Lib/site.py \
		Lib/sysconfig.py \
		Lib/test/test_site.py \
		Makefile.pre.in \
		Modules/Setup.dist \
		Modules/getpath.c \
		setup.py || die "sed failed to replace @@GENTOO_LIBDIR@@"

	epatch_user

	eautoconf
	eautoheader
}

src_configure() {
	# Disable extraneous modules with extra dependencies.
	export PYTHON_DISABLE_MODULES="dbm _bsddb gdbm _curses _curses_panel readline _sqlite3 _tkinter"
	export PYTHON_DISABLE_SSL="1"

	if [[ -n "${PYTHON_DISABLE_MODULES}" ]]; then
		einfo "Disabled modules: ${PYTHON_DISABLE_MODULES}"
	fi

	if [[ "$(gcc-major-version)" -ge 4 ]]; then
		append-flags -fwrapv
	fi

	filter-flags -malign-double

	[[ "${ARCH}" == "alpha" ]] && append-flags -fPIC

	# https://bugs.gentoo.org/show_bug.cgi?id=50309
	if is-flagq -O3; then
		is-flagq -fstack-protector-all && replace-flags -O3 -O2
		use hardened && replace-flags -O3 -O2
	fi

	if tc-is-cross-compiler; then
		# Force some tests that try to poke fs paths.
		export ac_cv_file__dev_ptc=no
		export ac_cv_file__dev_ptmx=yes
	fi

	# Export CXX so it ends up in /usr/lib/python2.X/config/Makefile.
	tc-export CXX
	# The configure script fails to use pkg-config correctly.
	# http://bugs.python.org/issue15506
	export ac_cv_path_PKG_CONFIG=$(tc-getPKG_CONFIG)

	# Set LDFLAGS so we link modules with -lpython2.7 correctly.
	# Needed on FreeBSD unless Python 2.7 is already installed.
	# Please query BSD team before removing this!
	append-ldflags "-L."

	BUILD_DIR="${WORKDIR}/${CHOST}"
	mkdir -p "${BUILD_DIR}" || die
	cd "${BUILD_DIR}" || die

	ECONF_SOURCE="${S}" OPT="" \
	econf \
		--prefix=/usr/share/oem/python \
		--with-fpectl \
		--disable-shared \
		--enable-ipv6 \
		--enable-threads \
		--enable-unicode=ucs4 \
		--includedir='/discard/include' \
		--infodir='/discard/info' \
		--mandir='/discard/man' \
		--with-dbmliborder="" \
		--with-libc="" \
		--without-system-expat \
		--without-system-ffi
}

src_compile() {
	# Avoid invoking pgen for cross-compiles.
	touch Include/graminit.h Python/graminit.c

	cd "${BUILD_DIR}" || die
	emake
}

src_install() {
	local bindir=/usr/share/oem/python/bin
	local libdir=/usr/share/oem/python/$(get_libdir)/python${SLOT}

	cd "${BUILD_DIR}" || die
	emake DESTDIR="${D}" altinstall

	# create a simple versionless 'python' symlink
	dosym "python${SLOT}" "${bindir}/python"

	# Throw away headers and man/info pages and extra modules
	rm -r "${D}/discard" \
		"${D}${bindir}/"{idle,smtpd.py} \
		"${D}${libdir}/"{bsddb,dbhash.py,test/test_bsddb*} \
		"${D}${libdir}/"{idlelib,lib-tk} \
		"${D}${libdir}/distutils/command/"wininst-*.exe \
		"${D}${libdir}/test" \
		"${D}${libdir}/sqlite3" \
		|| die
}
