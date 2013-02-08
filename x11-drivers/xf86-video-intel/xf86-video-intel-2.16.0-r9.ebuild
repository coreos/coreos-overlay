# Copyright 1999-2011 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-drivers/xf86-video-intel/xf86-video-intel-2.16.0.ebuild,v 1.1 2011/08/11 15:51:43 chithanh Exp $

EAPI=4

XORG_DRI=dri
inherit linux-info xorg-2

DESCRIPTION="X.Org driver for Intel cards"

KEYWORDS="amd64 ~ia64 x86 -x86-fbsd"
IUSE="dri sna xvmc broken_partialswaps"

RDEPEND="x11-libs/libXext
	x11-libs/libXfixes
	xvmc? (
		x11-libs/libXvMC
	)
	>=x11-libs/libxcb-1.5
	>=x11-libs/libdrm-2.4.23[video_cards_intel]
	sna? (
		>=x11-base/xorg-server-1.10
	)"
DEPEND="${RDEPEND}
	>=x11-proto/dri2proto-2.6"

PATCHES=(
 	# Copy the initial framebuffer contents when starting X so we can get
 	# seamless transitions.
	"${FILESDIR}/2.16.0-copy-fb.patch"
 	# Prevent X from touching boot-time gamma settings.
	"${FILESDIR}/2.14.0-no-gamma.patch"
	# BLT ring hang fix.
	"${FILESDIR}/2.16.0-blt-hang.patch"
	# Disable backlight adjustments on DPMS mode changes.
	"${FILESDIR}/2.16.0-no-backlight.patch"
	# Avoid display corruption when unable to flip
	"${FILESDIR}/2.16.0-fix-blt-damage.patch"
	# Split framebuffer and flip crtcs indepenently.
	"${FILESDIR}/2.16.0-per-crtc-flip.patch"
)

src_prepare() {
	# Disable triple buffering since we need double buffering
	# to implement partial updates on top of flips
	if use broken_partialswaps; then
		PATCHES+=(
		"${FILESDIR}/2.16.0-no-triple.patch"
		)
	fi

	for patch_file in "${PATCHES[@]}"; do
		epatch $patch_file
	done
}

pkg_setup() {
	xorg-2_pkg_setup
	CONFIGURE_OPTIONS="$(use_enable dri) --disable-xvmc --enable-kms-only"
}

pkg_postinst() {
	if linux_config_exists \
		&& ! linux_chkconfig_present DRM_I915_KMS; then
		echo
		ewarn "This driver requires KMS support in your kernel"
		ewarn "  Device Drivers --->"
		ewarn "    Graphics support --->"
		ewarn "      Direct Rendering Manager (XFree86 4.1.0 and higher DRI support)  --->"
		ewarn "      <*>   Intel 830M, 845G, 852GM, 855GM, 865G (i915 driver)  --->"
		ewarn "              i915 driver"
		ewarn "      [*]       Enable modesetting on intel by default"
		echo
	fi
}
