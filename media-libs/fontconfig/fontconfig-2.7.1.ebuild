# Copyright 1999-2009 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-libs/fontconfig/fontconfig-2.7.1-r1.ebuild,v 1.1 2009/08/23 22:48:21 dirtyepic Exp $

EAPI="2"

inherit eutils libtool toolchain-funcs flag-o-matic

DESCRIPTION="A library for configuring and customizing font access"
HOMEPAGE="http://fontconfig.org/"
SRC_URI="http://fontconfig.org/release/${P}.tar.gz"

LICENSE="fontconfig"
SLOT="1.0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~m68k ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~sparc-fbsd ~x86-fbsd"
IUSE="cros_host doc -highdpi -is_desktop"

# Purposefully dropped the xml USE flag and libxml2 support. Having this is
# silly since expat is the preferred way to go per upstream and libxml2 support
# simply exists as a fallback when expat isn't around. expat support is the main
# way to go and every other distro uses it. By using the xml USE flag to enable
# libxml2 support, this confuses users and results in most people getting the
# non-standard behavior of libxml2 usage since most profiles have USE=xml

RDEPEND=">=media-libs/freetype-2.2.1
	>=dev-libs/expat-1.95.3"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	doc? (
		app-text/docbook-sgml-utils[jadetex]
		=app-text/docbook-sgml-dtd-3.1*
	)"
PDEPEND="app-admin/eselect-fontconfig"

# Checks that a passed-in fontconfig default symlink (e.g. "10-autohint.conf")
# is present and dies if it isn't.
check_fontconfig_default() {
	local path="${D}"/etc/fonts/conf.d/"$1"
	if [ ! -L "$path" ]; then
		die "Didn't find $1 among default fontconfig settings (at $path)."
	fi
}

src_prepare() {
	epatch "${FILESDIR}"/${P}-latin-reorder.patch	#130466
	epatch "${FILESDIR}"/${P}-fonts-config.patch
	epatch "${FILESDIR}"/${P}-metric-aliases.patch
	epatch "${FILESDIR}"/${P}-conf-d.patch
	epunt_cxx	#74077

	# Needed to get a sane .so versioning on fbsd, please dont drop
	# If you have to run eautoreconf, you can also leave the elibtoolize call as
	# it will be a no-op.
	elibtoolize
}

src_configure() {
	local myconf
	if tc-is-cross-compiler; then
		myconf="--with-arch=${ARCH}"
		replace-flags -mtune=* -DMTUNE_CENSORED
		replace-flags -march=* -DMARCH_CENSORED
		filter-flags -mfpu=* -mfloat-abi=*
	fi
	# Make the global font cache be /usr/share/cache/fontconfig
	# by passing /usr/share for the localstatedir
	econf $(use_enable doc docs) \
		--localstatedir=/usr/share \
		--with-docdir=/usr/share/doc/${PF} \
		--with-default-fonts=/usr/share/fonts \
		--with-add-fonts=/usr/local/share/fonts \
		${myconf} || die
}

src_install() {
	emake DESTDIR="${D}" install || die

	#fc-lang directory contains language coverage datafiles
	#which are needed to test the coverage of fonts.
	insinto /usr/share/fc-lang
	doins fc-lang/*.orth

	insinto /etc/fonts
	doins "${S}"/fonts.conf
	doins "${FILESDIR}"/local.conf

	# Test that fontconfig's defaults for basic rendering settings match what we
	# want to use.
	check_fontconfig_default 10-autohint.conf
	check_fontconfig_default 10-hinting.conf
	check_fontconfig_default 10-hinting-slight.conf
	check_fontconfig_default 10-sub-pixel-rgb.conf

	# There's a lot of variability across different displays with subpixel
	# rendering.  Until we have a better solution, turn it off and use grayscale
	# instead on desktop boards.  Also disable it on high-DPI displays, since
	# they have little need for it and use subpixel positioning, which can
	# interact poorly with it (http://crbug.com/125066#c8).  Additionally,
	# disable it when installing to the host sysroot so the images in the
	# initramfs package won't use subpixel rendering (http://crosbug.com/27872).
	if use is_desktop || use highdpi || use cros_host; then
		rm "${D}"/etc/fonts/conf.d/10-sub-pixel-rgb.conf
		dosym ../conf.avail/10-no-sub-pixel.conf /etc/fonts/conf.d/.
		check_fontconfig_default 10-no-sub-pixel.conf
	fi

	# Disable hinting on high-DPI displays, where we're already using subpixel
	# positioning.
	if use highdpi; then
		rm "${D}"/etc/fonts/conf.d/10-hinting.conf
		dosym ../conf.avail/10-unhinted.conf /etc/fonts/conf.d/.
		check_fontconfig_default 10-unhinted.conf
	fi

	doman $(find "${S}" -type f -name *.1 -print)
	newman doc/fonts-conf.5 fonts.conf.5
	dodoc doc/fontconfig-user.{txt,pdf}

	if use doc; then
		doman doc/Fc*.3
		dohtml doc/fontconfig-devel.html
		dodoc doc/fontconfig-devel.{txt,pdf}
	fi

	dodoc AUTHORS ChangeLog README || die

	# Changes should be made to /etc/fonts/local.conf, and as we had
	# too much problems with broken fonts.conf, we force update it ...
	# <azarah@gentoo.org> (11 Dec 2002)
	echo 'CONFIG_PROTECT_MASK="/etc/fonts/fonts.conf"' > "${T}"/37fontconfig
	doenvd "${T}"/37fontconfig

	# As of fontconfig 2.7, everything sticks their noses in here.
	dodir /etc/sandbox.d
	echo 'SANDBOX_PREDICT="/usr/share/cache/fontconfig"' > "${D}"/etc/sandbox.d/37fontconfig
}

pkg_postinst() {
	einfo "Cleaning broken symlinks in "${ROOT}"etc/fonts/conf.d/"
	find -L "${ROOT}"etc/fonts/conf.d/ -type l -delete

	echo
	ewarn "Please make fontconfig configuration changes using \`eselect fontconfig\`"
	ewarn "Any changes made to /etc/fonts/fonts.conf will be overwritten."
	ewarn
	ewarn "If you need to reset your configuration to upstream defaults, delete"
	ewarn "the directory ${ROOT}etc/fonts/conf.d/ and re-emerge fontconfig."
	echo

	if [[ ${ROOT} = / ]]; then
		ebegin "Creating global font cache"
		/usr/bin/fc-cache -sr
		eend $?
	fi
}
