# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/app-emulation/open-vm-tools/open-vm-tools-2013.09.16.1328054-r3.ebuild,v 1.1 2014/02/02 15:47:56 floppym Exp $

EAPI=5

AUTOTOOLS_AUTORECONF=1
AUTOTOOLS_IN_SOURCE_BUILD=1

inherit autotools-utils eutils multilib pam user versionator flag-o-matic systemd toolchain-funcs

MY_PV="$(replace_version_separator 3 '-')"
MY_P="${PN}-${MY_PV}"

DESCRIPTION="Opensourced tools for VMware guests"
HOMEPAGE="http://open-vm-tools.sourceforge.net/"
SRC_URI="mirror://sourceforge/${PN}/${MY_P}.tar.gz"

LICENSE="LGPL-2"
SLOT="0"
KEYWORDS="amd64 ~x86"
IUSE="X doc fuse +dnet icu modules pam +pic xinerama"

COMMON_DEPEND="
	dev-libs/glib:2
	dnet? ( dev-libs/libdnet )
	sys-apps/ethtool
	sys-process/procps
	pam? ( virtual/pam )
	X? (
		dev-cpp/gtkmm:2.4
		x11-base/xorg-server
		x11-drivers/xf86-input-vmmouse
		x11-drivers/xf86-video-vmware
		x11-libs/gtk+:2
		x11-libs/libnotify
		x11-libs/libX11
		x11-libs/libXtst
	)
	fuse? ( sys-fs/fuse )
	icu? ( dev-libs/icu:= )
	xinerama? ( x11-libs/libXinerama )
"

DEPEND="${COMMON_DEPEND}
	doc? ( app-doc/doxygen )
	virtual/pkgconfig
	virtual/linux-sources
	sys-apps/findutils
"

RDEPEND="${COMMON_DEPEND}
	modules? ( app-emulation/open-vm-tools-kmod )
"

S="${WORKDIR}/${MY_P}"

PATCHES=(
	"${FILESDIR}/0001-add-extra-configure-flags.patch"
	"${FILESDIR}/0002-rename-dnet-config.patch"
	"${FILESDIR}/0003-oliver-test.patch"
)

pkg_setup() {
	enewgroup vmware
}

src_prepare() {
	autotools-utils_src_prepare
	# Do not filter out Werror
	# Upstream Bug  http://sourceforge.net/tracker/?func=detail&aid=2959749&group_id=204462&atid=989708
	# sed -i -e 's/CFLAGS=.*Werror/#&/g' configure || die "sed comment out Werror failed"
	sed -i -e 's:\(TEST_PLUGIN_INSTALLDIR=\).*:\1\$libdir/open-vm-tools/plugins/tests:g' configure || die "sed test_plugin_installdir failed"
}

src_configure() {
	# http://bugs.gentoo.org/402279
	if has_version '>=sys-process/procps-3.3.2'; then
		export CUSTOM_PROCPS_NAME=procps
		export CUSTOM_PROCPS_LIBS="$($(tc-getPKG_CONFIG) --libs libprocps)"
	fi

	local myeconfargs=(
		--disable-hgfs-mounter
		--with-procps
		--without-kernel-modules
		$(use_enable doc docs)
		--docdir=/usr/share/doc/${PF}
		$(use_with X x)
		$(use_with X gtk2)
		$(use_with X gtkmm)
		$(use_with dnet)
		$(use_with icu)
		$(use_with pam)
		$(use_with pic)
		$(use_enable xinerama multimon)
	)

	econf "${myeconfargs[@]}"

	# Bugs 260878, 326761
	find ./ -name Makefile | xargs sed -i -e 's/-Werror//g'  || die "sed out Werror failed"
}

src_install() {
	default

	rm "${D}"/etc/pam.d/vmtoolsd
	pamd_mimic_system vmtoolsd auth account

	rm "${D}"/usr/$(get_libdir)/*.la
	rm "${D}"/usr/$(get_libdir)/open-vm-tools/plugins/common/*.la

	systemd_dounit "${FILESDIR}"/vmtoolsd.service

	#exeinto /etc/vmware-tools/scripts/vmware/
	#doexe "${FILESDIR}"/network

	if use X;
	then
		fperms 4755 "/usr/bin/vmware-user-suid-wrapper"

		dobin "${S}"/scripts/common/vmware-xdg-detect-de

		#insinto /etc/xdg/autostart
		#doins "${FILESDIR}/open-vm-tools.desktop"

		elog "To be able to use the drag'n'drop feature of VMware for file"
		elog "exchange, please add the users to the 'vmware' group."
	fi
	elog "Add 'vmware-tools' service to the default runlevel."
}
