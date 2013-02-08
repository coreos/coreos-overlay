# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-gfx/imagemagick/imagemagick-6.5.8.8.ebuild,v 1.6 2010/03/04 19:55:56 armin76 Exp $

EAPI="2"

inherit eutils multilib perl-app toolchain-funcs versionator flag-o-matic autotools

MY_PN=ImageMagick
MY_P=${MY_PN}-${PV%.*}
MY_P2=${MY_PN}-${PV%.*}-${PV#*.*.*.}

DESCRIPTION="A collection of tools and libraries for many image formats"
HOMEPAGE="http://www.imagemagick.org/"
SRC_URI="mirror://imagemagick/${MY_P2}.tar.bz2
		 mirror://imagemagick/legacy/${MY_P2}.tar.bz2"

# perl tests fail with userpriv
RESTRICT="perl? ( userpriv )"
LICENSE="imagemagick"
SLOT="0"
KEYWORDS="alpha amd64 arm ~hppa ia64 ppc ppc64 s390 sh sparc x86"
IUSE="autotrace bzip2 +corefonts djvu doc fftw fontconfig fpx graphviz gs hdri
	jbig jpeg jpeg2k lcms lqr nocxx openexr openmp perl png q8 q32 raw svg tiff
	truetype X wmf xml zlib"

RDEPEND="
	autotrace? ( >=media-gfx/autotrace-0.31.1 )
	bzip2? ( app-arch/bzip2 )
	djvu? ( app-text/djvu )
	fftw? ( sci-libs/fftw )
	fontconfig? ( media-libs/fontconfig )
	fpx? ( media-libs/libfpx )
	graphviz? ( >=media-gfx/graphviz-2.6 )
	gs? ( app-text/ghostscript-gpl )
	jbig? ( media-libs/jbigkit )
	jpeg? ( virtual/jpeg )
	jpeg2k? ( media-libs/jasper )
	lcms? ( >=media-libs/lcms-1.06 )
	lqr? ( >=media-libs/liblqr-0.1.0 )
	openexr? ( media-libs/openexr )
	perl? ( >=dev-lang/perl-5.8.6-r6 )
	png? ( media-libs/libpng )
	raw? ( media-gfx/ufraw )
	tiff? ( >=media-libs/tiff-3.5.5 )
	truetype? ( =media-libs/freetype-2*
		corefonts? ( media-fonts/corefonts ) )
	wmf? ( >=media-libs/libwmf-0.2.8 )
	xml? ( >=dev-libs/libxml2-2.4.10 )
	zlib? ( sys-libs/zlib )
	X? (
		x11-libs/libXext
		x11-libs/libXt
		x11-libs/libICE
		x11-libs/libSM
		svg? ( >=gnome-base/librsvg-2.9.0 )
	)
	!dev-perl/perlmagick
	!media-gfx/graphicsmagick[imagemagick]
	!sys-apps/compare
	>=sys-devel/libtool-1.5.2-r6"

DEPEND="${RDEPEND}
	>=sys-apps/sed-4
	X? ( x11-proto/xextproto )"

S="${WORKDIR}/${MY_P2}"

pkg_setup() {
	# for now, only build svg support when X is enabled, as librsvg
	# pulls in quite some X dependencies.
	if use svg && ! use X ; then
		elog "the svg USE-flag requires the X USE-flag set."
		elog "disabling svg support for now."
	fi

	if use corefonts && ! use truetype ; then
		elog "corefonts USE-flag requires the truetype USE-flag to be set."
		elog "disabling corefonts support for now."
	fi
}

src_prepare() {
	epatch "${FILESDIR}"/${PN}-6.7.2.6-lfs.patch
	eautoreconf
	# fix doc dir, bug #91911
	sed -i -e \
		's:DOCUMENTATION_PATH="${DATA_DIR}/doc/${DOCUMENTATION_RELATIVE_PATH}":DOCUMENTATION_PATH="/usr/local/share/doc/${PF}":g' \
		"${S}"/configure || die
}

src_configure() {
	append-flags -L"${ROOT}"/usr/$(get_libdir)

	local myconf
	if use q32 ; then
		myconf="${myconf} --with-quantum-depth=32"
	elif use q8 ; then
		myconf="${myconf} --with-quantum-depth=8"
	else
		myconf="${myconf} --with-quantum-depth=16"
	fi

	if use X && use svg ; then
		myconf="${myconf} --with-rsvg"
	else
		myconf="${myconf} --without-rsvg"
	fi

	# openmp support only works with >=sys-devel/gcc-4.3, bug #223825
	if use openmp && version_is_at_least 4.3 $(gcc-version) ; then
		if has_version =sys-devel/gcc-$(gcc-version)*[openmp] ; then
			myconf="${myconf} --enable-openmp"
		else
			elog "disabling openmp support (requires >=sys-devel/gcc-4.3 with USE='openmp')"
			myconf="${myconf} --disable-openmp"
		fi
	else
		elog "disabling openmp support (requires >=sys-devel/gcc-4.3)"
		myconf="${myconf} --disable-openmp"
	fi

	use truetype && myconf="${myconf} $(use_with corefonts windows-font-dir /usr/share/fonts/corefonts)"

	econf \
		${myconf} \
		--without-included-ltdl \
		--with-ltdl-include=/usr/include \
		--with-ltdl-lib=/usr/$(get_libdir) \
		--with-threads \
		--with-modules \
		$(use_with perl) \
		--with-perl-options='INSTALLDIRS=vendor' \
		--with-gs-font-dir=/usr/share/fonts/default/ghostscript \
		$(use_enable hdri) \
		$(use_with !nocxx magick-plus-plus) \
		$(use_with autotrace) \
		$(use_with bzip2 bzlib) \
		$(use_with djvu) \
		$(use_with fftw) \
		$(use_with fontconfig) \
		$(use_with fpx) \
		$(use_with gs dps) \
		$(use_with gs gslib) \
		$(use_with graphviz gvc) \
		$(use_with jbig) \
		$(use_with jpeg jpeg) \
		$(use_with jpeg2k jp2) \
		$(use_with lcms) \
		$(use_with openexr) \
		$(use_with png) \
		$(use_with svg rsvg) \
		$(use_with tiff) \
		$(use_with truetype freetype) \
		$(use_with wmf) \
		$(use_with xml) \
		$(use_with zlib) \
		$(use_with X x) \
		--prefix=/usr/local \
		--mandir=/usr/local/share/man \
		--infodir=/usr/local/share/info \
		--datadir=/usr/local/share
}

src_test() {
	einfo "please note that the tests will only be run when the installed"
	einfo "version and current emerging version are the same"

	if has_version ~${CATEGORY}/${P} ; then
		emake -j1 check || die "make check failed"
	fi
}

src_install() {
	emake DESTDIR="${D}" install || die "Installation of files into image failed"

	elog "Preserving .la files needed at runtime by placing in tar file."
	elog "  Avoids removal due to *.la in INSTALL_MASK"
	pushd "${D}"/usr/local/$(get_libdir) || die
	tar zcf "${S}"/la_files.tar.gz $(find -name '*.la' -type f) || die
	popd
	insinto /usr/local/$(get_libdir)/${P}
	doins "${S}"/la_files.tar.gz

	# dont need these files with runtime plugins
	rm -f "${D}"/usr/local/$(get_libdir)/*/*/*.{la,a}

	use doc || rm -r "${D}"/usr/local/share/doc/${PF}/{www,images,index.html}
	dodoc NEWS.txt ChangeLog AUTHORS.txt README.txt

	# Fix perllocal.pod file collision
	use perl && fixlocalpod
}

pkg_postinst() {
	elog "Restoring .la files from tar file"
	pushd "${ROOT}"/usr/local/$(get_libdir) || die
	tar -xvzpf ${P}/la_files.tar.gz || die
	popd
}

pkg_prerm() {
	elog "Clean up untarred .la files"
	pushd "${ROOT}"/usr/local/$(get_libdir) || die
	tar -tf ${P}/la_files.tar.gz | xargs rm
	assert
	popd
}
