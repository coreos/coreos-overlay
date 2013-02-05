# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2
#
# Determine which systems must use the old (v2) preamble header format, and
# create a config file forcing that format for those systems.
#
# This is only needed to provide backward compatibility for already-shipping
# systems.

EAPI="4"

inherit cros-board

DESCRIPTION="Chrome OS verified boot tools config"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="amd64 arm x86"

# These are the ONLY systems that should require v2. All others should adapt
# to the new format.
OLD_BOARDS=(
	lumpy
	lumpy64
	stumpy
	stumpy64
	tegra2
	x86-alex
	x86-alex32
	x86-mario
	x86-mario64
	x86-zgb
	x86-zgb32
)

S=${WORKDIR}

src_compile() {
	local b

	b=$(get_current_board_no_variant)
	mkdir -p "config"
	if has "${b}" "${OLD_BOARDS[@]}" ; then
		fmt=2
	else
		fmt=3
	fi
        printf -- '--format\n%s\n' "${fmt}" > "config/vbutil_firmware.options"
        printf -- '--format\n%s\n' "${fmt}" > "config/vbutil_kernel.options"
}

src_install() {
	insinto /usr/share/vboot/config
	doins config/vbutil_{firmware,kernel}.options
}
