# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/media-sound/alsa-utils/alsa-utils-1.0.25-r1.ebuild,v 1.7 2012/04/01 15:13:17 armin76 Exp $

EAPI=4
inherit base systemd

MY_P=${P/_rc/rc}

DESCRIPTION="Advanced Linux Sound Architecture Utils (alsactl, alsamixer, etc.)"
HOMEPAGE="http://www.alsa-project.org/"
SRC_URI="mirror://alsaproject/utils/${MY_P}.tar.bz2
	!minimal? ( mirror://alsaproject/driver/alsa-driver-${PV}.tar.bz2 )"

LICENSE="GPL-2"
SLOT="0.9"
KEYWORDS="alpha amd64 arm hppa ia64 ~mips ~ppc ppc64 sh sparc x86"
IUSE="doc +libsamplerate minimal +ncurses nls"

COMMON_DEPEND=">=media-libs/alsa-lib-1.0.25
	libsamplerate? ( media-libs/libsamplerate )
	ncurses? ( >=sys-libs/ncurses-5.1 )"
RDEPEND="${COMMON_DEPEND}
	!minimal? (
		dev-util/dialog
		sys-apps/pciutils
	)"
DEPEND="${COMMON_DEPEND}
	doc? ( app-text/xmlto )"

S="${WORKDIR}/${MY_P}"
PATCHES=(
"${FILESDIR}/${PN}-1.0.23-modprobe.d.patch"
"${FILESDIR}/${P}-separate-usr-var-fs.patch"
"${FILESDIR}/${P}-Do-not-access-other-cards-than-specified-for.patch"
)

src_configure() {
	local myconf=""
	use doc || myconf="--disable-xmlto"

	econf ${myconf} \
		$(use_enable libsamplerate alsaloop) \
		$(use_enable nls) \
		$(use_enable ncurses alsamixer) \
		$(use_enable !minimal alsaconf) \
		"$(systemd_with_unitdir)"
}

src_install() {
	local ALSA_UTILS_DOCS="ChangeLog README TODO
		seq/aconnect/README.aconnect
		seq/aseqnet/README.aseqnet"

	emake DESTDIR="${D}" install || die "emake install failed"

	dodoc ${ALSA_UTILS_DOCS} || die

	use minimal || newbin "${WORKDIR}/alsa-driver-${PV}/utils/alsa-info.sh" \
		alsa-info

	newinitd "${FILESDIR}/alsasound.initd-r5" alsasound
	newconfd "${FILESDIR}/alsasound.confd-r4" alsasound
	insinto /etc/modprobe.d
	newins "${FILESDIR}/alsa-modules.conf-rc" alsa.conf

	keepdir /var/lib/alsa
}

pkg_postinst() {
	echo
	elog "To take advantage of the init script, and automate the process of"
	elog "saving and restoring sound-card mixer levels you should"
	elog "add alsasound to the boot runlevel. You can do this as"
	elog "root like so:"
	elog "	# rc-update add alsasound boot"
	echo
	ewarn "The ALSA core should be built into the kernel or loaded through other"
	ewarn "means. There is no longer any modular auto(un)loading in alsa-utils."
	echo
	if use minimal; then
		ewarn "The minimal use flag disables the dependency on pciutils that"
		ewarn "is needed by alsaconf at runtime."
	fi
}
