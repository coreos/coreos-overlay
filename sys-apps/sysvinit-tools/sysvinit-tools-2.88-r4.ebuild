# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils toolchain-funcs flag-o-matic

MY_PN="sysvinit"
MY_P="${MY_PN}-${PV}"

DESCRIPTION="/sbin/init - parent of all processes"
HOMEPAGE="http://savannah.nongnu.org/projects/sysvinit"
SRC_URI="mirror://nongnu/${MY_PN}/${MY_P}dsf.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="selinux ibm static"

RDEPEND="selinux? ( >=sys-libs/libselinux-1.28 )"
DEPEND="${RDEPEND}
	virtual/os-headers
	!sys-apps/sysvinit"

ONLY_INSTALL_REGEXP="last\\|mesg\\|wall\\|lastb\\|killall5\\|fstab-decode\\|pidof"

S=${WORKDIR}/${MY_P}dsf

src_unpack() {
	unpack ${A}
	cd "${S}"
	epatch "${FILESDIR}"/${MY_P}-makefile.patch #319197
	epatch "${FILESDIR}"/${MY_P}-selinux.patch #326697
	sed -i '/^CPPFLAGS =$/d' src/Makefile
}

src_compile() {
	local myconf

	tc-export CC
	append-lfs-flags
	use static && append-ldflags -static
	use selinux && myconf=WITH_SELINUX=yes
	emake -C src ${myconf} || die
}

src_install() {
	emake -C src install ROOT="${D}" || die
	find "${D}" -xtype f | grep -v "${ONLY_INSTALL_REGEXP}" | xargs /bin/rm -f 
	rmdir "${D}"/usr/include
	rmdir "${D}"/usr/share/man/man5
	dodoc "${FILESDIR}"/README
}

pkg_postinst() {
	ewarn "This is an stripped down version of sysvinit which only"
	ewarn "installs the following programs:"
	ewarn ""
	ewarn "/usr/bin/last"
	ewarn "/usr/bin/mesg"
	ewarn "/usr/bin/wall"
	ewarn "/usr/bin/lastb"
	ewarn "/sbin/killall5"
	ewarn "/sbin/fstab-decode"
	ewarn "/bin/pidof"
	ewarn ""
	ewarn "See: https://bugs.gentoo.org/show_bug.cgi?id=399615"
	ewarn ""
	ewarn "It is not guaranteed to work, and it will probably eat"
	ewarn "your system and make it unusable. Use at your own risk."
}
