# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/multilib-build.eclass,v 1.1 2013/02/01 21:39:50 mgorny Exp $

# @ECLASS: multilib-build.eclass
# @MAINTAINER:
# Michał Górny <mgorny@gentoo.org>
# @BLURB: flags and utility functions for building multilib packages
# @DESCRIPTION:
# The multilib-build.eclass exports USE flags and utility functions
# necessary to build packages for multilib in a clean and uniform
# manner.
#
# Please note that dependency specifications for multilib-capable
# dependencies shall use the USE dependency string in ${MULTILIB_USEDEP}
# to properly request multilib enabled.

if [[ ! ${_MULTILIB_BUILD} ]]; then

# EAPI=5 is required for meaningful MULTILIB_USEDEP.
case ${EAPI:-0} in
	5) ;;
	*) die "EAPI=${EAPI} is not supported" ;;
esac

inherit multilib multiprocessing

# @ECLASS-VARIABLE: _MULTILIB_FLAGS
# @INTERNAL
# @DESCRIPTION:
# The list of multilib flags and corresponding ABI values.
_MULTILIB_FLAGS=(
	abi_x86_32:x86
	abi_x86_64:amd64
)

# @ECLASS-VARIABLE: MULTILIB_USEDEP
# @DESCRIPTION:
# The USE-dependency to be used on dependencies (libraries) needing
# to support multilib as well.
#
# Example use:
# @CODE
# RDEPEND="dev-libs/libfoo[${MULTILIB_USEDEP}]
#	net-libs/libbar[ssl,${MULTILIB_USEDEP}]"
# @CODE

_multilib_build_set_globals() {
	local flags=( "${_MULTILIB_FLAGS[@]%:*}" )
	local usedeps=${flags[@]/%/(-)?}

	IUSE=${flags[*]}
	MULTILIB_USEDEP=${usedeps// /,}
}
_multilib_build_set_globals

# @FUNCTION: multilib_get_enabled_abis
# @DESCRIPTION:
# Return the ordered list of enabled ABIs if multilib builds
# are enabled. The best (most preferred) ABI will come last.
#
# If multilib is disabled, the default ABI will be returned
# in order to enforce consistent testing with multilib code.
multilib_get_enabled_abis() {
	debug-print-function ${FUNCNAME} "${@}"

	local abis=( $(get_all_abis) )

	local abi i found
	for abi in "${abis[@]}"; do
		for i in "${_MULTILIB_FLAGS[@]}"; do
			local m_abi=${i#*:}
			local m_flag=${i%:*}

			if [[ ${m_abi} == ${abi} ]] && use "${m_flag}"; then
				echo "${abi}"
				found=1
			fi
		done
	done

	if [[ ! ${found} ]]; then
		debug-print "${FUNCNAME}: no ABIs enabled, fallback to ${DEFAULT_ABI}"
		echo ${DEFAULT_ABI}
	fi
}

# @FUNCTION: multilib_foreach_abi
# @USAGE: <argv>...
# @DESCRIPTION:
# If multilib support is enabled, sets the toolchain up for each
# supported ABI along with the ABI variable and correct BUILD_DIR,
# and runs the given commands with them.
#
# If multilib support is disabled, it just runs the commands. No setup
# is done.
multilib_foreach_abi() {
	local initial_dir=${BUILD_DIR:-${S}}

	local ABI
	for ABI in $(multilib_get_enabled_abis); do
		multilib_toolchain_setup "${ABI}"
		BUILD_DIR=${initial_dir%%/}-${ABI} "${@}"
	done
}

# @FUNCTION: multilib_parallel_foreach_abi
# @USAGE: <argv>...
# @DESCRIPTION:
# If multilib support is enabled, sets the toolchain up for each
# supported ABI along with the ABI variable and correct BUILD_DIR,
# and runs the given commands with them. The commands are run
# in parallel with number of jobs being determined from MAKEOPTS.
#
# If multilib support is disabled, it just runs the commands. No setup
# is done.
#
# Useful for running configure scripts.
multilib_parallel_foreach_abi() {
	local initial_dir=${BUILD_DIR:-${S}}

	multijob_init

	local ABI
	for ABI in $(multilib_get_enabled_abis); do
		(
			multijob_child_init

			multilib_toolchain_setup "${ABI}"
			BUILD_DIR=${initial_dir%%/}-${ABI}
			"${@}"
		) &

		multijob_post_fork
	done

	multijob_finish
}

_MULTILIB_BUILD=1
fi
