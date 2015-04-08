# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Library for handling building of ChromiumOS packages
#
#
#  This class sets the BOARD environment variable.  It is intended to
#  be used by ebuild packages that need to have the board information
#  for various reasons -- for example, to differentiate various
#  hardware attributes at build time; see 'adhd' for an example of
#  this.
#
#  When set, the BOARD environment variable should conform to the
#  syntax used by the CROS utilities.
#
#  If an unknown board is encountered, this class deliberately fails
#  the build; this provides an easy method of identifying a change to
#  the build which might affect inheriting packages.
#
#  For example, the ADHD (Google A/V daemon) package relies on the
#  BOARD variable to determine which hardware should be monitored at
#  runtime.  If the BOARD variable is not set, the daemon will not
#  monitor any specific hardware; thus, when a new board is added, the
#  ADHD project must be updated.
#
[[ ${EAPI} != "4" ]] && die "Only EAPI=4 is supported"

BOARD_USE_PREFIX="board_use_"
ALL_BOARDS=(
	amd64-generic
	amd64-host
	arm64-generic
)

# Add BOARD_USE_PREFIX to each board in ALL_BOARDS to create IUSE.
# Also add cros_host so that we can inherit this eclass in ebuilds
# that get emerged both in the cros-sdk and for target boards.
# See REQUIRED_USE below.
IUSE="${ALL_BOARDS[@]/#/${BOARD_USE_PREFIX}} cros_host"

# Require one, and only one, of the board_use flags to be set if this eclass
# is used.  This effectively makes any ebuild that inherits this eclass require
# a known valid board to be set.
REQUIRED_USE="^^ ( ${IUSE} )"

# Echo the current board, with variant.
get_current_board_with_variant()
{
	local b

	for b in "${ALL_BOARDS[@]}"; do
		if use ${BOARD_USE_PREFIX}${b}; then
			echo ${b}
			return
		fi
	done

	die "Unable to determine current board (with variant)."
}

# Echo the current board, without variant.
get_current_board_no_variant()
{
	get_current_board_with_variant | cut -d '_' -f 1
}
