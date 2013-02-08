# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/x11-base/xorg-server/xorg-server-1.12.2.ebuild,v 1.9 2012/07/22 12:54:50 chithanh Exp $

EAPI=4

XORG_DOC=doc
XORG_EAUTORECONF="yes"
inherit flag-o-matic xorg-2 multilib versionator
EGIT_REPO_URI="git://anongit.freedesktop.org/git/xorg/xserver"

DESCRIPTION="X.Org X servers"
KEYWORDS="~alpha amd64 arm hppa ~ia64 ~mips ppc ppc64 ~sh ~sparc x86 ~amd64-fbsd ~x86-fbsd"

IUSE_SERVERS="dmx kdrive xnest xorg xvfb"
# +suid needed because sparcs default off
IUSE="${IUSE_SERVERS} broken_partialswaps -doc ipv6 minimal nptl selinux +suid tegra tslib +udev"

RDEPEND=">=app-admin/eselect-opengl-1.0.8
	dev-libs/openssl
	media-libs/freetype
	>=x11-apps/iceauth-1.0.2
	>=x11-apps/rgb-1.0.3
	>=x11-apps/xauth-1.0.3
	x11-apps/xkbcomp
	>=x11-libs/libpciaccess-0.12.901
	>=x11-libs/libXau-1.0.4
	>=x11-libs/libXdmcp-1.0.2
	>=x11-libs/libXfont-1.4.2
	>=x11-libs/libxkbfile-1.0.4
	>=x11-libs/pixman-0.21.8
	>=x11-libs/xtrans-1.2.2
	>=x11-misc/xbitmaps-1.0.1
	>=x11-misc/xkeyboard-config-2.4.1-r3
	dmx? (
		x11-libs/libXt
		>=x11-libs/libdmx-1.0.99.1
		>=x11-libs/libX11-1.1.5
		>=x11-libs/libXaw-1.0.4
		>=x11-libs/libXext-1.0.99.4
		>=x11-libs/libXfixes-5.0
		>=x11-libs/libXi-1.2.99.1
		>=x11-libs/libXmu-1.0.3
		x11-libs/libXrender
		>=x11-libs/libXres-1.0.3
		>=x11-libs/libXtst-1.0.99.2
	)
	kdrive? (
		>=x11-libs/libXext-1.0.5
		x11-libs/libXv
	)
	!minimal? (
		>=x11-libs/libX11-1.1.5
		>=x11-libs/libXext-1.0.5
		>=media-libs/mesa-7.8_rc[nptl=]
	)
	tslib? ( >=x11-libs/tslib-1.0 )
	udev? ( >=sys-fs/udev-150 )
	>=x11-apps/xinit-1.3
	selinux? ( sec-policy/selinux-xserver )"

DEPEND="${RDEPEND}
	sys-devel/flex
	>=x11-proto/bigreqsproto-1.1.0
	>=x11-proto/compositeproto-0.4
	>=x11-proto/damageproto-1.1
	>=x11-proto/fixesproto-5.0
	>=x11-proto/fontsproto-2.0.2
	>=x11-proto/glproto-1.4.14
	>=x11-proto/inputproto-2.1.99.3
	>=x11-proto/kbproto-1.0.3
	>=x11-proto/randrproto-1.2.99.3
	>=x11-proto/recordproto-1.13.99.1
	>=x11-proto/renderproto-0.11
	>=x11-proto/resourceproto-1.0.2
	>=x11-proto/scrnsaverproto-1.1
	>=x11-proto/trapproto-3.4.3
	>=x11-proto/videoproto-2.2.2
	>=x11-proto/xcmiscproto-1.2.0
	>=x11-proto/xextproto-7.1.99
	>=x11-proto/xf86dgaproto-2.0.99.1
	>=x11-proto/xf86rushproto-1.1.2
	>=x11-proto/xf86vidmodeproto-2.2.99.1
	>=x11-proto/xineramaproto-1.1.3
	>=x11-proto/xproto-7.0.22
	dmx? (
		>=x11-proto/dmxproto-2.2.99.1
		doc? (
			|| (
				www-client/links
				www-client/lynx
				www-client/w3m
			)
		)
	)
	!minimal? (
		>=x11-proto/xf86driproto-2.1.0
		>=x11-proto/dri2proto-2.6
		>=x11-libs/libdrm-2.4.20
	)"

PDEPEND="
	xorg? ( >=x11-base/xorg-drivers-$(get_version_component_range 1-2) )"

REQUIRED_USE="!minimal? (
		|| ( ${IUSE_SERVERS} )
	)"

#UPSTREAMED_PATCHES=(
#	"${WORKDIR}/patches/"
#)

PATCHES=(
	"${UPSTREAMED_PATCHES[@]}"
	"${FILESDIR}"/${PN}-1.12-disable-acpi.patch

	# Allow usage of monotonic clock while cross-compiling.
	"${FILESDIR}/1.11.99.902-monotonic-clock-fix.patch"
	"${FILESDIR}/1.11.99.902-cache-xkbcomp-for-fast-start-up.patch"
	# Match the behaviour of monitor_reconfigure at X.Org startup time.
	"${FILESDIR}/1.9.3-chromeos-mode.patch"
	# Allow setting the root window background to nothing to further reduce
	# flicker when showing and hiding the composite overlay window.
	"${FILESDIR}/1.11.99.902-allow-root-none.patch"
	# Refcount glxdrawables to avoid crashes on double free()
	"${FILESDIR}/1.11.99.902-refcnt-glxdrawable.patch"

	"${FILESDIR}/1.11.99.902-chromium-mouse-accel-profile.patch"

	"${FILESDIR}/1.11.99.902-xserver-bg-none-root.patch"
	# Dont load a default X cursor.
	"${FILESDIR}/1.9.3-no-default-cursor.patch"
	# Ability to run without root privs
	"${FILESDIR}/1.11.99.902-nohwaccess.patch"
	# Don't attend clients which are already gone, race condition in dri2
	"${FILESDIR}/1.12.0-do-not-attend-gone-clients.patch"
	# crosbug.com/p/11408
	"${FILESDIR}/1.12.0-fix-scale-to-desktop-for-edge-ABS-events.patch"
	# Fix for xfixes pointer barriers
	"${FILESDIR}/1.12.0-xfixes-safer-barriers.patch"
	# crosbug.com/33813
	"${FILESDIR}/1.12.0-suffix-match-udev-paths.patch"
	# crosbug.com/31570
	"${FILESDIR}/1.12.0-os-block-signals-when-accessing-global-timer-list.patch"
	# Fix for crash with floating touchscreen (http://crosbug.com/27529)
	"${FILESDIR}/1.12.0-mi-don-t-check-for-core-events-in-miPointerSetPositi.patch"
	# Add the flag to specify the maximum VT the user can switch to
	"${FILESDIR}/1.12.0-add-maxvt-flag.patch"
	# Save touchpoint last coordinates before transform
	"${FILESDIR}/1.13.0-dix-Save-touchpoint-last-coordinates-before-transform.patch"
	# crbug.com/30822
	"${FILESDIR}/1.12.4-Per-Randr-CRTC-pointer-scaling.patch"
)

src_prepare() {
	# Partial flips
	if use broken_partialswaps; then
		PATCHES+=(
		"${FILESDIR}/1.12.0-emulate-partial-flips.patch"
		)
	fi
	xorg-2_src_prepare
}

pkg_pretend() {
	# older gcc is not supported
	[[ "${MERGE_TYPE}" != "binary" && $(gcc-major-version) -lt 4 ]] && \
		die "Sorry, but gcc earlier than 4.0 wont work for xorg-server."
}

pkg_setup() {
	xorg-2_pkg_setup

	# localstatedir is used for the log location; we need to override the default
	#	from ebuild.sh
	# sysconfdir is used for the xorg.conf location; same applies
	# NOTE: fop is used for doc generating ; and i have no idea if gentoo
	#	package it somewhere
	XORG_CONFIGURE_OPTIONS=(
		$(use_enable ipv6)
		$(use_enable dmx)
		$(use_enable kdrive)
		$(use_enable kdrive kdrive-kbd)
		$(use_enable kdrive kdrive-mouse)
		$(use_enable kdrive kdrive-evdev)
		$(use_enable tslib)
		$(use_enable !minimal record)
		$(use_enable !minimal xfree86-utils)
		$(use_enable !minimal install-libxf86config)
		$(use_enable !tegra dri)
		$(use_enable !tegra dri2)
		$(use_enable !arm glx)
		$(use_enable !arm vgahw)
		$(use_enable !arm vbe)
		$(use_enable xnest)
		$(use_enable xorg)
		$(use_enable xvfb)
		$(use_enable nptl glx-tls)
		$(use_enable udev config-udev)
		$(use_with doc doxygen)
		$(use_with doc xmlto)
		$(use_enable suid install-setuid)
		--sysconfdir=/etc/X11
		--localstatedir=/var
		--with-fontrootdir=/usr/share/fonts
		--with-xkb-output=/var/lib/xkb
		--disable-config-hal
		--without-dtrace
		--without-fop
		--with-os-vendor=Gentoo
		--with-sha1=libcrypto
		--disable-xvmc
		--disable-xdmcp
		--disable-screensaver
		--disable-xdm-auth-1
		--disable-dbe
		--disable-xinerama
		--disable-dga
		--disable-xace
		--disable-config-dbus
		--disable-config-hal
		--disable-clientids
		--disable-xf86vidmode
		--disable-registry
		--disable-xfake
		--disable-dmx
		--disable-xvfb
		--disable-xnest
		--enable-null-root-cursor
		--with-default-font-path=built-ins
	)

	if use amd64 || use x86 ; then
		XORG_CONFIGURE_OPTIONS+=( --enable-xaa)
	else
		XORG_CONFIGURE_OPTIONS+=( --disable-xaa)
	fi

	# Things we may want to remove later:
	#	--disable-xaa (requires dropping all xaa drivers)
	#	--disable-xv (requires fixing the drivers)
	#	--disable-tcp-transport
	#	--disable-ipv6
	#	--disable-secure-rpc

	# Xorg-server requires includes from OS mesa which are not visible for
	# users of binary drivers.
	mkdir -p "${T}/mesa-symlinks/GL"
	for i in gl glx glxmd glxproto glxtokens; do
		ln -s "${EROOT}usr/$(get_libdir)/opengl/xorg-x11/include/$i.h" "${T}/mesa-symlinks/GL/$i.h" || die
	done
	for i in glext glxext; do
		ln -s "${EROOT}usr/$(get_libdir)/opengl/global/include/$i.h" "${T}/mesa-symlinks/GL/$i.h" || die
	done
	append-cppflags "-I${T}/mesa-symlinks"

	# Make breakage less obvious, bug #402285.
	replace-flags -O3 -O2
}

src_install() {
	xorg-2_src_install

	dynamic_libgl_install

	server_based_install

	if ! use minimal &&	use xorg; then
		# Install xorg.conf.example into docs
		dodoc "${AUTOTOOLS_BUILD_DIR}"/hw/xfree86/xorg.conf.example
	fi

	newinitd "${FILESDIR}"/xdm-setup.initd-1 xdm-setup
	newinitd "${FILESDIR}"/xdm.initd-8 xdm
	newconfd "${FILESDIR}"/xdm.confd-4 xdm

	# install the @x11-module-rebuild set for Portage
	insinto /usr/share/portage/config/sets
	newins "${FILESDIR}"/xorg-sets.conf xorg.conf

	# crosbug.com/11553
	dosym /usr/bin/Xorg /usr/bin/X11/X || die
}

pkg_postinst() {
	# sets up libGL and DRI2 symlinks if needed (ie, on a fresh install)
	eselect opengl set xorg-x11 --use-old

	if [[ ${PV} != 9999 && $(get_version_component_range 2 ${REPLACING_VERSIONS}) != $(get_version_component_range 2 ${PV}) ]]; then
		ewarn "You must rebuild all drivers if upgrading from <xorg-server-$(get_version_component_range 1-2)"
		ewarn "because the ABI changed. If you cannot start X because"
		ewarn "of module version mismatch errors, this is your problem."

		echo
		ewarn "You can generate a list of all installed packages in the x11-drivers"
		ewarn "category using this command:"
		ewarn "	emerge portage-utils; qlist -I -C x11-drivers/"
		ewarn "or using sets from portage-2.2:"
		ewarn "	emerge @x11-module-rebuild"
	fi

	if use udev && has_version sys-fs/udev[-keymap]; then
		ewarn "sys-fs/udev was built without keymap support. This may cause input device"
		ewarn "autoconfiguration to fail."
	fi
}

pkg_postrm() {
	# Get rid of module dir to ensure opengl-update works properly
	if [[ -z ${REPLACED_BY_VERSION} && -e ${ROOT}/usr/$(get_libdir)/xorg/modules ]]; then
		rm -rf "${ROOT}"/usr/$(get_libdir)/xorg/modules
	fi
}

dynamic_libgl_install() {
	# next section is to setup the dynamic libGL stuff
	ebegin "Moving GL files for dynamic switching"
		dodir /usr/$(get_libdir)/opengl/xorg-x11/extensions
		local x=""
		for x in "${D}"/usr/$(get_libdir)/xorg/modules/extensions/lib{glx,dri,dri2}*; do
			if [ -f ${x} -o -L ${x} ]; then
				mv -f ${x} "${D}"/usr/$(get_libdir)/opengl/xorg-x11/extensions
			fi
		done
	eend 0
}

server_based_install() {
	if ! use xorg; then
		rm "${D}"/usr/share/man/man1/Xserver.1x \
			"${D}"/usr/$(get_libdir)/xserver/SecurityPolicy \
			"${D}"/usr/$(get_libdir)/pkgconfig/xorg-server.pc \
			"${D}"/usr/share/man/man1/Xserver.1x
	fi
}
