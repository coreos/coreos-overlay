# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/python-single-r1.eclass,v 1.15 2013/01/30 10:42:25 mgorny Exp $

# @ECLASS: python-single-r1
# @MAINTAINER:
# Python team <python@gentoo.org>
# @AUTHOR:
# Author: Michał Górny <mgorny@gentoo.org>
# Based on work of: Krzysztof Pawlik <nelchael@gentoo.org>
# @BLURB: An eclass for Python packages not installed for multiple implementations.
# @DESCRIPTION:
# An extension of the python-r1 eclass suite for packages which
# don't support being installed for multiple Python implementations.
# This mostly includes tools embedding Python.
#
# This eclass extends the IUSE and REQUIRED_USE set by python-r1
# to request correct PYTHON_SINGLE_TARGET. It also replaces
# PYTHON_USEDEP and PYTHON_DEPS with a more suitable form.
#
# Please note that packages support multiple Python implementations
# (using python-r1 eclass) can not depend on packages not supporting
# them (using this eclass).
#
# Please note that python-single-r1 will always inherit python-utils-r1
# as well. Thus, all the functions defined there can be used
# in the packages using python-single-r1, and there is no need ever
# to inherit both.
#
# For more information, please see the python-r1 Developer's Guide:
# http://www.gentoo.org/proj/en/Python/python-r1/dev-guide.xml

case "${EAPI:-0}" in
	0|1|2|3|4)
		die "Unsupported EAPI=${EAPI:-0} (too old) for ${ECLASS}"
		;;
	5)
		# EAPI=5 is required for meaningful USE default deps
		# on USE_EXPAND flags
		;;
	*)
		die "Unsupported EAPI=${EAPI} (unknown) for ${ECLASS}"
		;;
esac

if [[ ! ${_PYTHON_SINGLE_R1} ]]; then

if [[ ${_PYTHON_R1} ]]; then
	die 'python-single-r1.eclass can not be used with python-r1.eclass.'
elif [[ ${_PYTHON_ANY_R1} ]]; then
	die 'python-single-r1.eclass can not be used with python-any-r1.eclass.'
fi

inherit python-utils-r1

fi

EXPORT_FUNCTIONS pkg_setup

if [[ ! ${_PYTHON_SINGLE_R1} ]]; then

# @ECLASS-VARIABLE: PYTHON_COMPAT
# @REQUIRED
# @DESCRIPTION:
# This variable contains a list of Python implementations the package
# supports. It must be set before the `inherit' call. It has to be
# an array.
#
# Example:
# @CODE
# PYTHON_COMPAT=( python{2_5,2_6,2_7} )
# @CODE
if ! declare -p PYTHON_COMPAT &>/dev/null; then
	die 'PYTHON_COMPAT not declared.'
fi

# @ECLASS-VARIABLE: PYTHON_REQ_USE
# @DEFAULT_UNSET
# @DESCRIPTION:
# The list of USEflags required to be enabled on the chosen Python
# implementations, formed as a USE-dependency string. It should be valid
# for all implementations in PYTHON_COMPAT, so it may be necessary to
# use USE defaults.
#
# Example:
# @CODE
# PYTHON_REQ_USE="gdbm,ncurses(-)?"
# @CODE
#
# It will cause the Python dependencies to look like:
# @CODE
# python_single_target_pythonX_Y? ( dev-lang/python:X.Y[gdbm,ncurses(-)?] )
# @CODE

# @ECLASS-VARIABLE: PYTHON_DEPS
# @DESCRIPTION:
# This is an eclass-generated Python dependency string for all
# implementations listed in PYTHON_COMPAT.
#
# The dependency string is conditional on PYTHON_SINGLE_TARGET.
#
# Example use:
# @CODE
# RDEPEND="${PYTHON_DEPS}
#	dev-foo/mydep"
# DEPEND="${RDEPEND}"
# @CODE
#
# Example value:
# @CODE
# dev-python/python-exec
# python_single_target_python2_6? ( dev-lang/python:2.6[gdbm] )
# python_single_target_python2_7? ( dev-lang/python:2.7[gdbm] )
# @CODE

# @ECLASS-VARIABLE: PYTHON_USEDEP
# @DESCRIPTION:
# This is an eclass-generated USE-dependency string which can be used to
# depend on another Python package being built for the same Python
# implementations.
#
# The generate USE-flag list is compatible with packages using python-r1,
# python-single-r1 and python-distutils-ng eclasses. It must not be used
# on packages using python.eclass.
#
# Example use:
# @CODE
# RDEPEND="dev-python/foo[${PYTHON_USEDEP}]"
# @CODE
#
# Example value:
# @CODE
# python_targets_python2_7(-)?,python_single_target_python2_7(+)?
# @CODE

_python_single_set_globals() {
	local impls=()

	PYTHON_DEPS=
	local i PYTHON_PKG_DEP
	for i in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${i}" || continue

		# The chosen targets need to be in PYTHON_TARGETS as well.
		# This is in order to enforce correct dependencies on packages
		# supporting multiple implementations.
		#REQUIRED_USE+=" python_single_target_${i}? ( python_targets_${i} )"

		python_export "${i}" PYTHON_PKG_DEP
		PYTHON_DEPS+="python_single_target_${i}? ( ${PYTHON_PKG_DEP} ) "

		impls+=( "${i}" )
	done

	if [[ ${#impls[@]} -eq 0 ]]; then
		die "No supported implementation in PYTHON_COMPAT."
	fi

	local flags_mt=( "${impls[@]/#/python_targets_}" )
	local flags=( "${impls[@]/#/python_single_target_}" )

	local optflags=${flags_mt[@]/%/(-)?}
	optflags+=,${flags[@]/%/(+)?}

	IUSE="${flags_mt[*]} ${flags[*]}"
	#REQUIRED_USE="|| ( ${flags_mt[*]} ) ^^ ( ${flags[*]} )"
	PYTHON_USEDEP=${optflags// /,}

	# 1) well, python-exec would suffice as an RDEP
	# but no point in making this overcomplex, BDEP doesn't hurt anyone
	# 2) python-exec should be built with all targets forced anyway
	# but if new targets were added, we may need to force a rebuild
	PYTHON_DEPS+="dev-python/python-exec[${PYTHON_USEDEP}]"
}
_python_single_set_globals

# @FUNCTION: python-single-r1_pkg_setup
# @DESCRIPTION:
# Determine what the selected Python implementation is and set EPYTHON
# and PYTHON accordingly.
python-single-r1_pkg_setup() {
	debug-print-function ${FUNCNAME} "${@}"

	unset EPYTHON

	local impl
	for impl in "${_PYTHON_ALL_IMPLS[@]}"; do
		if has "${impl}" "${PYTHON_COMPAT[@]}" \
			&& use "python_single_target_${impl}"
		then
			if [[ ${EPYTHON} ]]; then
				eerror "Your PYTHON_SINGLE_TARGET setting lists more than a single Python"
				eerror "implementation. Please set it to just one value. If you need"
				eerror "to override the value for a single package, please use package.env"
				eerror "or an equivalent solution (man 5 portage)."
				echo
				die "More than one implementation in PYTHON_SINGLE_TARGET."
			fi

			if ! use "python_targets_${impl}"; then
				eerror "The implementation chosen as PYTHON_SINGLE_TARGET must be added"
				eerror "to PYTHON_TARGETS as well. This is in order to ensure that"
				eerror "dependencies are satisfied correctly. We're sorry"
				eerror "for the inconvenience."
				echo
				die "Build target (${impl}) not in PYTHON_TARGETS."
			fi

			python_export "${impl}" EPYTHON PYTHON
		fi
	done

	if [[ ! ${EPYTHON} ]]; then
		eerror "No Python implementation selected for the build. Please set"
		eerror "the PYTHON_SINGLE_TARGET variable in your make.conf to one"
		eerror "of the following values:"
		eerror
		eerror "${PYTHON_COMPAT[@]}"
		echo
		die "No supported Python implementation in PYTHON_SINGLE_TARGET."
	fi
}

# @FUNCTION: python_fix_shebang
# @USAGE: <path>...
# @DESCRIPTION:
# Replace the shebang in Python scripts with the current Python
# implementation (EPYTHON). If a directory is passed, works recursively
# on all Python scripts.
#
# Only files having a 'python' shebang will be modified; other files
# will be skipped. If a script has a complete shebang matching
# the chosen interpreter version, it is left unmodified. If a script has
# a complete shebang matching other version, the command dies.
python_fix_shebang() {
	debug-print-function ${FUNCNAME} "${@}"

	[[ ${1} ]] || die "${FUNCNAME}: no paths given"
	[[ ${EPYTHON} ]] || die "${FUNCNAME}: EPYTHON unset (pkg_setup not called?)"

	local path f
	for path; do
		while IFS= read -r -d '' f; do
			local shebang=$(head -n 1 "${f}")

			case "${shebang}" in
				'#!'*${EPYTHON}*)
					debug-print "${FUNCNAME}: in file ${f#${D}}"
					debug-print "${FUNCNAME}: shebang matches EPYTHON: ${shebang}"
					;;
				'#!'*python[23].[0123456789]*|'#!'*pypy-c*|'#!'*jython*)
					debug-print "${FUNCNAME}: in file ${f#${D}}"
					debug-print "${FUNCNAME}: incorrect specific shebang: ${shebang}"

					die "${f#${D}} has a specific Python shebang not matching EPYTHON"
					;;
				'#!'*python*)
					debug-print "${FUNCNAME}: in file ${f#${D}}"
					debug-print "${FUNCNAME}: rewriting shebang: ${shebang}"

					einfo "Fixing shebang in ${f#${D}}"
					_python_rewrite_shebang "${f}"
			esac
		done < <(find "${path}" -type f -print0)
	done
}

_PYTHON_SINGLE_R1=1
fi
