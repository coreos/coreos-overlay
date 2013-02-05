# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Eclass for use by ebuilds that need to know the debug serial port.
#
[[ ${EAPI} != "4" ]] && die "Only EAPI=4 is supported"

SERIAL_USE_PREFIX="serial_use_"
ALL_SERIALPORTS=(
	ttyAMA{0..5}
	ttyO{0..5}
	ttyS{0..5}
	ttySAC{0..5}
)
IUSE=${ALL_SERIALPORTS[@]/#/${SERIAL_USE_PREFIX}}

# Echo the current serial port name
get_serial_name() {
	local item

	for item in "${ALL_SERIALPORTS[@]}"; do
		if use ${SERIAL_USE_PREFIX}${item}; then
			echo ${item}
			return
		fi
	done

	die "Unable to determine current serial port."
}
