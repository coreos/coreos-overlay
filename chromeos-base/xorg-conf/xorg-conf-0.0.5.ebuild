# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# NOTE: This ebuild could be overridden in an overlay to provide a
# board-specific xorg.conf as necessary.

EAPI=4
inherit cros-board

DESCRIPTION="Board specific xorg configuration file."

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE="cmt elan -exynos synaptics -tegra"

S=${WORKDIR}

src_install() {
	insinto /etc/X11
	if ! use tegra; then
		doins "${FILESDIR}/xorg.conf"
	fi

	insinto /etc/X11/xorg.conf.d
	if use tegra; then
		doins "${FILESDIR}/tegra.conf"
	elif use exynos; then
		doins "${FILESDIR}/exynos.conf"
	fi

	# Since syntp does not use evdev (/dev/input/event*) device nodes,
	# its .conf snippet can be installed alongside one of the
	# evdev-compatible xf86-input-* touchpad drivers.
	if use synaptics; then
		doins "${FILESDIR}/50-touchpad-syntp.conf"
	fi

	if use cros_host; then
		# Install all cmt files when installing on the host
		doins "${FILESDIR}"/*.conf
	elif use cmt; then
		# install the cmt config file matching the current board
		doins "${FILESDIR}/50-touchpad-cmt.conf"

		local board=$(get_current_board_no_variant)
		doins "${FILESDIR}/50-touchpad-cmt-${board}.conf"

		# install cmt config file for elan, which is not a board
		if use elan; then
			doins "${FILESDIR}/50-touchpad-cmt-elan.conf"
		fi
	elif use board_use_mario; then
		doins "${FILESDIR}/50-touchpad-synaptics-mario.conf"
	else
		doins "${FILESDIR}/50-touchpad-synaptics.conf"
	fi
	doins "${FILESDIR}/20-mouse.conf"
	doins "${FILESDIR}/20-touchscreen.conf"
}
