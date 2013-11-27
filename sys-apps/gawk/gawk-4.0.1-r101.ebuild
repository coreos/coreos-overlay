# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-apps/gawk/gawk-4.0.1.ebuild,v 1.10 2012/09/27 02:02:47 blueness Exp $

EAPI="4"

inherit eutils toolchain-funcs multilib

DESCRIPTION="GNU awk pattern-matching language"
HOMEPAGE="http://www.gnu.org/software/gawk/gawk.html"
SRC_URI="mirror://gnu/gawk/${P}.tar.xz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm hppa ia64 m68k ~mips ppc ppc64 s390 sh sparc x86 ~ppc-aix ~amd64-fbsd ~sparc-fbsd ~x86-fbsd ~x64-freebsd ~x86-freebsd ~hppa-hpux ~ia64-hpux ~x86-interix ~amd64-linux ~ia64-linux ~x86-linux ~ppc-macos ~x64-macos ~x86-macos ~m68k-mint ~sparc-solaris ~sparc64-solaris ~x64-solaris ~x86-solaris"
IUSE="nls readline symlink-usr"

# older gawk's provided shared lib for baselayout-1
RDEPEND="!<sys-apps/baselayout-2.0.1
	readline? ( sys-libs/readline )"
DEPEND="${RDEPEND}
	nls? ( sys-devel/gettext )"

src_prepare() {
	# use symlinks rather than hardlinks, and disable version links
	sed -i \
		-e '/^LN =/s:=.*:= $(LN_S):' \
		-e '/install-exec-hook:/s|$|\nfoo:|' \
		Makefile.in doc/Makefile.in
	sed -i '/^pty1:$/s|$|\n_pty1:|' test/Makefile.in #413327
}

src_configure() {
	export ac_cv_libsigsegv=no
	econf \
		--libexec='$(libdir)/misc' \
		$(use_enable nls) \
		$(use_with readline)
}

src_install() {
	emake install DESTDIR="${D}" || die

	# Keep important gawk in /bin
	if use userland_GNU ; then
		if ! use symlink-usr ; then
			dodir /bin
			mv "${ED}"/usr/bin/gawk "${ED}"/bin/ || die
			dosym /bin/gawk /usr/bin/gawk
			# Provide canonical `awk`
			dosym gawk /bin/awk
		fi

		dosym gawk /usr/bin/awk
		dosym gawk.1 /usr/share/man/man1/awk.1
	fi

	# Install headers
	insinto /usr/include/awk
	doins *.h || die
	rm "${ED}"/usr/include/awk/config.h || die

	dodoc AUTHORS ChangeLog FUTURES LIMITATIONS NEWS PROBLEMS POSIX.STD README README_d/*.*
	for x in */ChangeLog ; do
		newdoc ${x} ${x##*/}.${x%%/*}
	done
}
