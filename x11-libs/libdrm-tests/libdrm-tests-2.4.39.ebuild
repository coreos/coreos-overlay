# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-libs/libdrm/libdrm-2.4.34.ebuild,v 1.1 2012/05/11 00:25:45 chithanh Exp $

EAPI=4
inherit xorg-2

EGIT_REPO_URI="git://anongit.freedesktop.org/git/mesa/drm"

UPSTREAM_PKG="${P/-tests}"

DESCRIPTION="X.Org libdrm library"
HOMEPAGE="http://dri.freedesktop.org/"
if [[ ${PV} = 9999* ]]; then
	SRC_URI=""
else
	SRC_URI="http://dri.freedesktop.org/${PN}/${UPSTREAM_PKG}.tar.bz2"
fi

KEYWORDS="~alpha amd64 arm ~hppa ~ia64 ~mips ~ppc ~ppc64 ~s390 ~sh ~sparc x86 ~amd64-fbsd ~x86-fbsd ~x64-freebsd ~x86-freebsd ~amd64-linux ~x86-linux ~sparc-solaris ~x64-solaris ~x86-solaris"
VIDEO_CARDS="exynos intel nouveau omap radeon vmware"
for card in ${VIDEO_CARDS}; do
	IUSE_VIDEO_CARDS+=" video_cards_${card}"
done

IUSE="${IUSE_VIDEO_CARDS} libkms"
RESTRICT="test" # see bug #236845

RDEPEND="dev-libs/libpthread-stubs
	sys-fs/udev
	video_cards_intel? ( >=x11-libs/libpciaccess-0.10 )
	~x11-libs/libdrm-${PV}"
DEPEND="${RDEPEND}"

S=${WORKDIR}/${UPSTREAM_PKG}

pkg_setup() {
	XORG_CONFIGURE_OPTIONS=(
		--enable-udev
		$(use_enable video_cards_intel intel)
		$(use_enable video_cards_nouveau nouveau)
		$(use_enable video_cards_radeon radeon)
		$(use_enable video_cards_vmware vmwgfx-experimental-api)
		$(use_enable video_cards_exynos exynos-experimental-api)
		$(use_enable video_cards_omap omap-experimental-api)
		$(use_enable libkms)
	)

	xorg-2_pkg_setup
}

src_compile() {
	xorg-2_src_compile

	# Manually build tests since they are not built automatically.
	# This should match the logic of tests/Makefile.am.  e.g. gem tests for
	# intel only.
	TESTS=( dr{i,m}stat )
	if use video_cards_intel; then
		TESTS+=( gem_{basic,flink,readwrite,mmap} )
	fi
	emake -C "${AUTOTOOLS_BUILD_DIR}"/tests "${TESTS[@]}"
}

src_install() {
	into /usr/local/
	dobin "${AUTOTOOLS_BUILD_DIR}"/tests/*/.libs/*
	dobin "${TESTS[@]/#/${AUTOTOOLS_BUILD_DIR}/tests/.libs/}"
}
