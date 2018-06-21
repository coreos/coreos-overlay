# Copyright 2018 CoreOS, Inc.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: coreos-sec-policy.eclass
# @BLURB: Container Linux sec-policy ebuild routines.

# When updating the sec-policy ebuilds BASEPOL needs to be updated to match
# the ${PVR} version string of the upstream selinux policy packages.
# BASEPOL must corespond to a published gentoo policy patchbundle.
export BASEPOL="2.20170805-r3"

# Setting these variables allows the sec-policy makefiles to work with
# Container Linux.
export BINDIR="/usr/bin"
export SHAREDIR="${EROOT:-/}usr/share/selinux"

# For build debugging.
export QUIET="n"

# Avoid circular dependency selinux-base <-> selinux-base-policy.
if [[ "${CATEGORY}/${PN}" != "sec-policy/selinux-base" &&
	"${CATEGORY}/${PN}" != "sec-policy/selinux-base-policy" ]]; then
	inherit selinux-policy-2
fi

coreos-sec-policy_pkg_postinst() {
	debug-print-function ${FUNCNAME} "$@"

	# For board builds Container Linux installs policy modules to the OS
	# image in the SDK build_image script.
	[[ "${CBUILD}" == "${CHOST}" ]] || return

	selinux-policy-2_pkg_postinst
}

EXPORT_FUNCTIONS pkg_postinst
