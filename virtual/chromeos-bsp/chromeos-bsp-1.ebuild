# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

DESCRIPTION="Generic ebuild which satisifies virtual/chromeos-bsp.
This is a direct dependency of chromeos-base/chromeos, but is expected to
be overridden in an overlay for each specialized board.  A typical
non-generic implementation will install any board-specific configuration
files and drivers which are not suitable for inclusion in a generic board
overlay."
HOMEPAGE="http://src.chromium.org"

LICENSE="BSD"
SLOT="0"
KEYWORDS="amd64 arm x86"
IUSE=""

X86_DEPEND="
	net-wireless/iwl1000-ucode
	net-wireless/iwl2000-ucode
	net-wireless/iwl2030-ucode
	net-wireless/iwl3945-ucode
	net-wireless/iwl4965-ucode
	>=net-wireless/iwl5000-ucode-8.24.2.12
	net-wireless/iwl6000-ucode
	net-wireless/iwl6005-ucode
	net-wireless/iwl6030-ucode
	net-wireless/iwl6050-ucode
"

RDEPEND="
	!chromeos-base/chromeos-bsp-null
	amd64? ( ${X86_DEPEND} )
	x86? ( ${X86_DEPEND} )
"
DEPEND="${RDEPEND}"
