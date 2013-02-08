# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/sys-block/parted/parted-3.1.ebuild,v 1.1 2012/03/04 18:23:21 jer Exp $

EAPI="3"

WANT_AUTOMAKE="1.11"

inherit autotools eutils

DESCRIPTION="Create, destroy, resize, check, copy partitions and file systems"
HOMEPAGE="http://www.gnu.org/software/parted"
SRC_URI="mirror://gnu/${PN}/${P}.tar.xz"

LICENSE="GPL-3"
SLOT="0"
KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86"
IUSE="+debug device-mapper nls readline selinux static-libs test"

# specific version for gettext needed
# to fix bug 85999
RDEPEND="
	>=sys-fs/e2fsprogs-1.27
	>=sys-libs/ncurses-5.2
	nls? ( >=sys-devel/gettext-0.12.1-r2 )
	readline? ( >=sys-libs/readline-5.2 )
	selinux? ( sys-libs/libselinux )
	device-mapper? ( || ( >=sys-fs/lvm2-2.02.45 sys-fs/device-mapper ) )
"
DEPEND="
	${RDEPEND}
	dev-util/pkgconfig
	test? ( >=dev-libs/check-0.9.3 )
"

src_prepare() {
	# Remove tests known to FAIL instead of SKIP without OS/userland support
	sed -i libparted/tests/Makefile.am \
		-e 's|t3000-symlink.sh||g' || die "sed failed"
	sed -i tests/Makefile.am \
		-e '/t4100-msdos-partition-limits.sh/d' \
		-e '/t4100-dvh-partition-limits.sh/d' \
		-e '/t6000-dm.sh/d' || die "sed failed"
	# there is no configure flag for controlling the dev-libs/check test
	sed -i configure.ac \
		-e "s:have_check=[a-z]*:have_check=$(usex test):g" || die

	# Fix a file descriptor leak due to fsync errors.
	# See crosbug.com/33674 for details.
	epatch "${FILESDIR}/${P}-fix-file-descriptor-leak.patch" || die

	eautoreconf
}

src_configure() {
	econf \
		$(use_with readline) \
		$(use_enable nls) \
		$(use_enable debug) \
		$(use_enable selinux) \
		$(use_enable device-mapper) \
		$(use_enable static-libs static) \
		--disable-rpath
}

src_test() {
	if use debug; then
		# Do not die when tests fail - some requirements are not
		# properly checked and should not lead to the ebuild failing.
		emake check
	else
		ewarn "Skipping tests because USE=-debug is set."
	fi
}

src_install() {
	emake install DESTDIR="${D}" || die "Install failed"
	dodoc AUTHORS BUGS ChangeLog NEWS README THANKS TODO
	dodoc doc/{API,FAT,USER.jp}
	find "${ED}" -name '*.la' -exec rm -f '{}' +
}
