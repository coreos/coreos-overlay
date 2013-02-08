# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/dev-java/icedtea6-bin/icedtea6-bin-1.6.2-r2.ebuild,v 1.3 2010/02/19 19:35:21 maekke Exp $

EAPI="1"

inherit java-vm-2

dist="mirror://gentoo/"
DESCRIPTION="A Gentoo-made binary build of the icedtea6 JDK"
TARBALL_VERSION="${PV}-r2"
SRC_URI="amd64? ( ${dist}/${PN}-core-${TARBALL_VERSION}-amd64.tar.bz2 )
	x86? ( ${dist}/${PN}-core-${TARBALL_VERSION}-x86.tar.bz2 )
	doc? ( ${dist}/${PN}-doc-${TARBALL_VERSION}.tar.bz2 )
	examples? (
		amd64? ( ${dist}/${PN}-examples-${TARBALL_VERSION}-amd64.tar.bz2 )
		x86? ( ${dist}/${PN}-examples-${TARBALL_VERSION}-x86.tar.bz2 )
	)
	nsplugin? (
		amd64? ( ${dist}/${PN}-nsplugin-${TARBALL_VERSION}-amd64.tar.bz2 )
		x86? ( ${dist}/${PN}-nsplugin-${TARBALL_VERSION}-x86.tar.bz2 )
	)
	source? ( ${dist}/${PN}-src-${TARBALL_VERSION}.tar.bz2 )"
HOMEPAGE="http://icedtea.classpath.org"

IUSE="X alsa doc examples nsplugin source"
RESTRICT="strip"

LICENSE="GPL-2-with-linking-exception"
SLOT="0"
KEYWORDS="amd64 x86"

S="${WORKDIR}/${PN}-${TARBALL_VERSION}"

RDEPEND=">=sys-devel/gcc-4.3
	>=sys-libs/glibc-2.9
	>=media-libs/giflib-4.1.6-r1
	virtual/jpeg
	>=media-libs/libpng-1.2.38
	>=sys-libs/zlib-1.2.3-r1
	alsa? ( >=media-libs/alsa-lib-1.0.20 )
	X? (
		>=media-libs/freetype-2.3.9:2
		>=media-libs/fontconfig-2.6.0-r2:1.0
		>=x11-libs/libXext-1.0.5
		>=x11-libs/libXi-1.2.1
		>=x11-libs/libXtst-1.0.3
		>=x11-libs/libX11-1.2.2
		x11-libs/libXt
	)
	nsplugin? (
		>=dev-libs/atk-1.26.0
		>=dev-libs/glib-2.20.5:2
		>=dev-libs/nspr-4.8
		>=x11-libs/cairo-1.8.8
		>=x11-libs/gtk+-2.16.6:2
		>=x11-libs/pango-1.24.5
	)"
DEPEND=""

QA_EXECSTACK_amd64="opt/${P}/jre/lib/amd64/server/libjvm.so"
QA_EXECSTACK_x86="opt/${P}/jre/lib/i386/server/libjvm.so
	opt/${P}/jre/lib/i386/client/libjvm.so"

src_install() {
	local dest="/opt/${P}"
	local ddest="${D}/${dest}"
	dodir "${dest}" || die

	local arch=${ARCH}

	# doins can't handle symlinks.
	cp -pRP bin include jre lib man "${ddest}" || die "failed to copy"

	dodoc ../doc/{ASSEMBLY_EXCEPTION,THIRD_PARTY_README} || die
	if use doc ; then
		dohtml -r ../doc/html/* || die "Failed to install documentation"
	fi

	if use examples; then
		cp -pRP share/{demo,sample} "${ddest}" || die
	fi

	if use source ; then
		cp src.zip "${ddest}" || die
	fi

	if use nsplugin ; then
		use x86 && arch=i386
		install_mozilla_plugin "${dest}/jre/lib/${arch}/IcedTeaPlugin.so"
	fi

	set_java_env
	java-vm_revdep-mask
}

pkg_postinst() {
	# Set as default VM if none exists
	java-vm-2_pkg_postinst

	if use nsplugin; then
		elog "The icedtea6-bin browser plugin can be enabled using eselect java-nsplugin"
		elog "Note that the plugin works only in browsers based on xulrunner-1.9.1"
		elog "such as Firefox 3.5, and not in other versions! xulrunner-1.9.2 (Firefox 3.6)"
		elog "is not supported by upstream yet."
	fi
}
