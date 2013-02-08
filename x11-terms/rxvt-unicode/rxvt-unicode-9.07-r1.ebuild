# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-terms/rxvt-unicode/rxvt-unicode-9.07-r1.ebuild,v 1.1 2010/02/03 05:07:45 jer Exp $

EAPI="2"

inherit autotools flag-o-matic

DESCRIPTION="rxvt clone with xft and unicode support"
HOMEPAGE="http://software.schmorp.de/pkg/rxvt-unicode.html"
SRC_URI="http://dist.schmorp.de/rxvt-unicode/${P}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~ppc ~ppc64 ~sparc x86 ~x86-fbsd"
IUSE="minimal +truetype perl iso14755 afterimage xterm-color wcwidth +vanilla"

# see bug #115992 for modular x deps
RDEPEND="x11-libs/libX11
	x11-libs/libXft
	afterimage? ( media-libs/libafterimage )
	x11-libs/libXrender
	perl? ( dev-lang/perl )
	>=sys-libs/ncurses-5.7-r3"
DEPEND="${RDEPEND}
	dev-util/pkgconfig
	x11-proto/xproto"

src_prepare() {
	if { use xterm-color || use wcwidth; }; then
		ewarn "You enabled xterm-color or wcwidth or both."
		ewarn "Please note that neither of them are supported by upstream."
		ewarn "You are at your own if you run into problems."
		ebeep 5
	fi

	local tdir=/usr/share/terminfo
	if use xterm-color; then
		epatch doc/urxvt-8.2-256color.patch
		sed -e \
			's/^\(rxvt-unicode\)/\1256/;s/colors#88/colors#256/;s/pairs#256/pairs#32767/' \
			doc/etc/rxvt-unicode.terminfo > doc/etc/rxvt-unicode256.terminfo
		sed -i -e \
			"s~^\(\s\+@TIC@.*\)~\1\n\t@TIC@ -o "${D}"/${tdir} \$(srcdir)/etc/rxvt-unicode256.terminfo~" \
			doc/Makefile.in
	fi

	# kill the rxvt-unicode terminfo file - #192083
	sed -i -e "/rxvt-unicode.terminfo/d" doc/Makefile.in ||
		die "sed failed"

	use wcwidth && epatch doc/wcwidth.patch

	# bug #240165
	epatch "${FILESDIR}"/${PN}-9.06-no-urgency-if-focused.diff

	# ncurses will provide rxvt-unicode terminfo, so we don't install them again
	# see bug #192083
	#
	# According to my tests this is not (yet?) true, so keep it prepared and
	# disabled  until it's needed again.
	#if has_version '<sys-libs/ncurses-5.7'; then
	sed -i -e \
		"s~@TIC@ \(\$(srcdir)/etc/rxvt\)~@TIC@ -o "${D}"/${tdir} \1~" \
		doc/Makefile.in
	#else
	#	# Remove everything except if we have rxvt-unicode256
	#	sed -i -e \
	#		'/rxvt-unicode256/p;/@TIC@/d' \
	#		doc/Makefile.in
	#fi

	# bug #263638
	epatch "${FILESDIR}"/${PN}-9.06-popups-hangs.patch

	# bug #237271
	if ! use vanilla; then
		ewarn "You are going to include third-party bug fixes/features."
		ewarn "They came without any warranty and are not supported by the"
		ewarn "Gentoo community."
		ebeep 5
		epatch "${FILESDIR}"/${PN}-9.05_no-MOTIF-WM-INFO.patch
		epatch "${FILESDIR}"/${PN}-9.06-font-width.patch
	fi

	eautoreconf
}

src_configure() {
	local myconf=''

	use iso14755 || myconf='--disable-iso14755'
	use xterm-color && myconf="$myconf --enable-xterm-colors=256"

	econf \
		$(use_enable truetype xft) \
		$(use_enable afterimage) \
		$(use_enable perl) \
		--disable-text-blink \
		--enable-font-styles \
		--disable-pixbuf \
		--disable-transparency \
		--disable-fading \
		--disable-utmp \
		--disable-wtmp \
		--disable-next-scroll \
		--disable-xterm-scroll \
		--disable-smart-resize \
		--with-x \
		${myconf}
}

src_compile() {
	emake || die "emake failed"

	sed -i \
		-e 's/RXVT_BASENAME = "rxvt"/RXVT_BASENAME = "urxvt"/' \
		"${S}"/doc/rxvt-tabbed || die "tabs sed failed"
}

src_install() {
	if use minimal; then
		# Default installation has urxvtc/urxvtd which are rarely used
		newbin src/rxvt urxvt || die
		return
	fi

	make DESTDIR="${D}" install || die

	dodoc README.FAQ Changes
	cd "${S}"/doc
	dodoc README* changes.txt etc/* rxvt-tabbed
}

pkg_postinst() {
	einfo "urxvt now always uses TERM=rxvt-unicode so that the"
	einfo "upstream-supplied terminfo files can be used."
}
