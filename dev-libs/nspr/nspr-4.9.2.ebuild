# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-libs/nspr/nspr-4.9.2.ebuild,v 1.8 2012/11/29 23:40:13 blueness Exp $

EAPI=3

inherit eutils multilib toolchain-funcs versionator

MIN_PV="$(get_version_component_range 2)"

DESCRIPTION="Netscape Portable Runtime"
HOMEPAGE="http://www.mozilla.org/projects/nspr/"
SRC_URI="ftp://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v${PV}/src/${P}.tar.gz"

LICENSE="|| ( MPL-1.1 GPL-2 LGPL-2.1 )"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ppc ppc64 sparc x86 ~ppc-aix ~amd64-fbsd ~x86-fbsd ~amd64-linux ~x86-linux ~x64-macos ~x86-macos ~sparc-solaris ~x64-solaris ~x86-solaris"
IUSE="debug"

src_prepare() {
	mkdir build inst
	epatch "${FILESDIR}"/${PN}-4.8-config.patch
	epatch "${FILESDIR}"/${PN}-4.6.1-config-1.patch
	epatch "${FILESDIR}"/${PN}-4.6.1-lang.patch
	epatch "${FILESDIR}"/${PN}-4.7.0-prtime.patch
	epatch "${FILESDIR}"/${PN}-4.7.1-solaris.patch
	epatch "${FILESDIR}"/${PN}-4.7.4-solaris.patch
	epatch "${FILESDIR}"/${PN}-4.8.3-aix-gcc.patch
	# Patch needs updating
	#epatch "${FILESDIR}"/${PN}-4.8.3-aix-soname.patch
	epatch "${FILESDIR}"/${PN}-4.8.4-darwin-install_name.patch
	epatch "${FILESDIR}"/${PN}-4.8.9-link-flags.patch
	epatch "${FILESDIR}"/${PN}-4.9.1-x32_v0.2.patch

	# ChromeOS: Fix x64 -> x86 cross-compilation
	epatch "${FILESDIR}"/${PN}-4.9.2-r1-cross.patch

	# make sure it won't find Perl out of Prefix
	sed -i -e "s/perl5//g" "${S}"/mozilla/nsprpub/configure || die

	# Respect LDFLAGS
	sed -i -e 's/\$(MKSHLIB) \$(OBJS)/\$(MKSHLIB) \$(LDFLAGS) \$(OBJS)/g' \
		"${S}"/mozilla/nsprpub/config/rules.mk || die
}

src_configure() {
	cd "${S}"/build

	echo > "${T}"/test.c
	$(tc-getCC) -c "${T}"/test.c -o "${T}"/test.o
	case $(file "${T}"/test.o) in
		*32-bit*x86-64*|*64-bit*|*ppc64*|*x86_64*) myconf="${myconf} --enable-64bit";;
		*32-bit*|*ppc*|*i386*) ;;
		*) die "Failed to detect whether your arch is 64bits or 32bits, disable distcc if you're using it, please";;
	esac

	myconf="${myconf} --libdir=${EPREFIX}/usr/$(get_libdir)"

	export HOST_CC="${HOSTCC}"
	export HOST_CFLAGS="-g"
	export HOST_LDFLAGS="-g"
	LC_ALL="C" HOST_CC="$HOST_CC" HOST_CFLAGS="$HOST_CFLAGS" \
		HOST_LDFLAGS="$HOST_LDFLAGS" ECONF_SOURCE="../mozilla/nsprpub" \
		econf \
		$(use_enable debug) \
		$(use_enable !debug optimize) \
		${myconf} || die "econf failed"
}

src_compile() {
	cd "${S}"/build
	if tc-is-cross-compiler; then
		emake CC="$(tc-getBUILD_CC)" CXX="$(tc-getBUILD_CXX)" \
			-C config nsinstall || die "failed to build"
		mv config/{,native-}nsinstall
		sed -s 's#/nsinstall$#/native-nsinstall#' -i config/autoconf.mk
		rm config/nsinstall.o
	fi
	emake CC="$(tc-getCC)" CXX="$(tc-getCXX)" || die "failed to build"
}

src_install () {
	# Their build system is royally confusing, as usual
	MINOR_VERSION=${MIN_PV} # Used for .so version
	cd "${S}"/build
	emake DESTDIR="${D}" install || die "emake install failed"

	cd "${ED}"/usr/$(get_libdir)
	for file in *.a; do
		einfo "removing static libraries as upstream has requested!"
		rm -f ${file} || die "failed to remove static libraries."
	done

	local n=
	# aix-soname.patch does this already
	[[ ${CHOST} == *-aix* ]] ||
	for file in *$(get_libname); do
		n=${file%$(get_libname)}$(get_libname ${MINOR_VERSION})
		mv ${file} ${n} || die "failed to mv files around"
		ln -s ${n} ${file} || die "failed to symlink files."
		if [[ ${CHOST} == *-darwin* ]]; then
			install_name_tool -id "${EPREFIX}/usr/$(get_libdir)/${n}" ${n} || die
		fi
	done

	# install nspr-config
	dobin "${S}"/build/config/nspr-config || die "failed to install nspr-config"

	# Remove stupid files in /usr/bin
	rm -f "${ED}"/usr/bin/prerr.properties || die "failed to cleanup unneeded files"
}

pkg_postinst() {
	ewarn
	ewarn "Please make sure you run revdep-rebuild after upgrade."
	ewarn "This is *extremely* important to ensure your system nspr works properly."
	ewarn
}
