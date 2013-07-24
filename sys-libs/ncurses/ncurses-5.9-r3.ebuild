# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-libs/ncurses/ncurses-5.9-r2.ebuild,v 1.17 2013/01/17 04:19:21 vapier Exp $

EAPI="1"
inherit eutils flag-o-matic toolchain-funcs

MY_PV=${PV:0:3}
PV_SNAP=${PV:4}
MY_P=${PN}-${MY_PV}
DESCRIPTION="console display library"
HOMEPAGE="http://www.gnu.org/software/ncurses/ http://dickey.his.com/ncurses/"
SRC_URI="mirror://gnu/ncurses/${MY_P}.tar.gz"

LICENSE="MIT"
SLOT="5"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~amd64-fbsd ~sparc-fbsd ~x86-fbsd"
IUSE="ada +cxx debug doc gpm minimal profile static-libs tinfo trace unicode"

DEPEND="gpm? ( sys-libs/gpm )"
#	berkdb? ( sys-libs/db )"
RDEPEND="${DEPEND}
	!<x11-terms/rxvt-unicode-9.06-r3"

S=${WORKDIR}/${MY_P}

src_unpack() {
	unpack ${A}
	cd "${S}"
	[[ -n ${PV_SNAP} ]] && epatch "${WORKDIR}"/${MY_P}-${PV_SNAP}-patch.sh
	epatch "${FILESDIR}"/${PN}-5.8-gfbsd.patch
	epatch "${FILESDIR}"/${PN}-5.7-nongnu.patch
	epatch "${FILESDIR}"/${PN}-5.9-rxvt-unicode-9.15.patch #192083 #383871
	epatch "${FILESDIR}"/${PN}-5.9-fix-clang-build.patch #417763
}

src_compile() {
	unset TERMINFO #115036
	tc-export_build_env BUILD_{CC,CPP}
	BUILD_CPPFLAGS+=" -D_GNU_SOURCE" #214642

	# when cross-compiling, we need to build up our own tic
	# because people often don't keep matching host/target
	# ncurses versions #249363
	if tc-is-cross-compiler && ! ROOT=/ has_version ~sys-libs/${P} ; then
		make_flags="-C progs tic"
		CHOST=${CBUILD} \
		CFLAGS=${BUILD_CFLAGS} \
		CXXFLAGS=${BUILD_CXXFLAGS} \
		CPPFLAGS=${BUILD_CPPFLAGS} \
		LDFLAGS="${BUILD_LDFLAGS} -static" \
		do_compile cross --without-shared --with-normal
	fi

	make_flags=""
	do_compile narrowc
	use unicode && do_compile widec --enable-widec --includedir=/usr/include/ncursesw
}
do_compile() {
	ECONF_SOURCE=${S}

	mkdir "${WORKDIR}"/$1
	cd "${WORKDIR}"/$1
	shift

	# ncurses is dumb and doesn't install .pc files unless pkg-config
	# is also installed.  Force the tests to go our way.  Note that it
	# doesn't actually use pkg-config ... it just looks for set vars.
	tc-export PKG_CONFIG
	export PKG_CONFIG_LIBDIR="/usr/$(get_libdir)/pkgconfig"

	# The chtype/mmask-t settings below are to retain ABI compat
	# with ncurses-5.4 so dont change em !
	local conf_abi="
		--with-chtype=long \
		--with-mmask-t=long \
		--disable-ext-colors \
		--disable-ext-mouse \
		--without-pthread \
		--without-reentrant \
	"
	# We need the basic terminfo files in /etc, bug #37026.  We will
	# add '--with-terminfo-dirs' and then populate /etc/terminfo in
	# src_install() ...
#		$(use_with berkdb hashed-db)
	econf \
		--with-terminfo-dirs="/etc/terminfo:/usr/share/terminfo" \
		--with-shared \
		--without-hashed-db \
		$(use_with ada) \
		$(use_with cxx) \
		$(use_with cxx cxx-binding) \
		$(use_with debug) \
		$(use_with profile) \
		$(use_with gpm) \
		--disable-termcap \
		--enable-symlinks \
		--with-rcs-ids \
		--with-manpage-format=normal \
		--enable-const \
		--enable-colorfgbg \
		--enable-echo \
		--enable-pc-files \
		$(use_enable !ada warnings) \
		$(use_with debug assertions) \
		$(use_enable debug leaks) \
		$(use_with debug expanded) \
		$(use_with !debug macros) \
		$(use_with trace) \
		$(use_with tinfo termlib) \
		${conf_abi} \
		"$@"

	# A little hack to fix parallel builds ... they break when
	# generating sources so if we generate the sources first (in
	# non-parallel), we can then build the rest of the package
	# in parallel.  This is not really a perf hit since the source
	# generation is quite small.
	emake -j1 sources || die
	# For some reason, sources depends on pc-files which depends on
	# compiled libraries which depends on sources which ...
	# Manually delete the pc-files file so the install step will
	# create the .pc files we want.
	rm -f misc/pc-files
	emake ${make_flags} || die
}

src_install() {
	# use the cross-compiled tic (if need be) #249363
	export PATH=${WORKDIR}/cross/progs:${PATH}

	# install unicode version second so that the binaries in /usr/bin
	# support both wide and narrow
	cd "${WORKDIR}"/narrowc
	emake DESTDIR="${D}" install || die
	if use unicode ; then
		cd "${WORKDIR}"/widec
		emake DESTDIR="${D}" install || die
	fi

	# Move libncurses{,w} into /lib
	gen_usr_ldscript -a \
		ncurses \
		$(usex unicode 'ncursesw' '') \
		$(use tinfo && usex unicode 'tinfow' '') \
		$(usev tinfo)
	ln -sf libncurses.so "${D}"/usr/$(get_libdir)/libcurses.so || die
	use static-libs || find "${D}"/usr/ -name '*.a' -a '!' -name '*curses++*.a' -delete

#	if ! use berkdb ; then
		# We need the basic terminfo files in /etc, bug #37026
		einfo "Installing basic terminfo files in /etc..."
		for x in ansi console dumb linux rxvt rxvt-256color rxvt-unicode \
				 screen screen-256color sun vt{52,100,102,200,220} \
				 xterm xterm-color xterm-256color xterm-xfree86
		do
			local termfile=$(find "${D}"/usr/share/terminfo/ -name "${x}" 2>/dev/null)
			local basedir=$(basename $(dirname "${termfile}"))

			if [[ -n ${termfile} ]] ; then
				dodir /etc/terminfo/${basedir}
				mv ${termfile} "${D}"/etc/terminfo/${basedir}/
				dosym ../../../../etc/terminfo/${basedir}/${x} \
					/usr/share/terminfo/${basedir}/${x}
			fi
		done

		# Build fails to create this ...
		dosym ../share/terminfo /usr/$(get_libdir)/terminfo
#	fi

	echo "CONFIG_PROTECT_MASK=\"/etc/terminfo\"" > "${T}"/50ncurses
	doenvd "${T}"/50ncurses

	if use minimal ; then
		rm -r "${D}"/usr/share/terminfo*
	fi

	# Because ncurses5-config --terminfo returns the directory we keep it
	keepdir /usr/share/terminfo #245374

	cd "${S}"
	dodoc ANNOUNCE MANIFEST NEWS README* TO-DO doc/*.doc
	use doc && dohtml -r doc/html/
}
