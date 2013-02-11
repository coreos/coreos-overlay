# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/python-r1.eclass,v 1.40 2013/01/30 10:42:25 mgorny Exp $

# @ECLASS: python-r1
# @MAINTAINER:
# Python team <python@gentoo.org>
# @AUTHOR:
# Author: Michał Górny <mgorny@gentoo.org>
# Based on work of: Krzysztof Pawlik <nelchael@gentoo.org>
# @BLURB: A common, simple eclass for Python packages.
# @DESCRIPTION:
# A common eclass providing helper functions to build and install
# packages supporting being installed for multiple Python
# implementations.
#
# This eclass sets correct IUSE and REQUIRED_USE. It exports PYTHON_DEPS
# and PYTHON_USEDEP so you can create correct dependencies for your
# package easily. It also provides methods to easily run a command for
# each enabled Python implementation and duplicate the sources for them.
#
# Please note that python-r1 will always inherit python-utils-r1 as
# well. Thus, all the functions defined there can be used
# in the packages using python-r1, and there is no need ever to inherit
# both.
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

if [[ ! ${_PYTHON_R1} ]]; then

if [[ ${_PYTHON_SINGLE_R1} ]]; then
	die 'python-r1.eclass can not be used with python-single-r1.eclass.'
elif [[ ${_PYTHON_ANY_R1} ]]; then
	die 'python-r1.eclass can not be used with python-any-r1.eclass.'
fi

inherit python-utils-r1

# @ECLASS-VARIABLE: PYTHON_COMPAT
# @REQUIRED
# @DESCRIPTION:
# This variable contains a list of Python implementations the package
# supports. It must be set before the `inherit' call. It has to be
# an array.
#
# Example:
# @CODE
# PYTHON_COMPAT=( python2_5 python2_6 python2_7 )
# @CODE
#
# Please note that you can also use bash brace expansion if you like:
# @CODE
# PYTHON_COMPAT=( python{2_5,2_6,2_7} )
# @CODE
if ! declare -p PYTHON_COMPAT &>/dev/null; then
	if [[ ${CATEGORY}/${PN} == dev-python/python-exec ]]; then
		PYTHON_COMPAT=( "${_PYTHON_ALL_IMPLS[@]}" )
	else
		die 'PYTHON_COMPAT not declared.'
	fi
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
# python_targets_pythonX_Y? ( dev-lang/python:X.Y[gdbm,ncurses(-)?] )
# @CODE

# @ECLASS-VARIABLE: PYTHON_DEPS
# @DESCRIPTION:
# This is an eclass-generated Python dependency string for all
# implementations listed in PYTHON_COMPAT.
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
# python_targets_python2_6? ( dev-lang/python:2.6[gdbm] )
# python_targets_python2_7? ( dev-lang/python:2.7[gdbm] )
# @CODE

# @ECLASS-VARIABLE: PYTHON_USEDEP
# @DESCRIPTION:
# This is an eclass-generated USE-dependency string which can be used to
# depend on another Python package being built for the same Python
# implementations.
#
# The generate USE-flag list is compatible with packages using python-r1
# and python-distutils-ng eclasses. It must not be used on packages
# using python.eclass.
#
# Example use:
# @CODE
# RDEPEND="dev-python/foo[${PYTHON_USEDEP}]"
# @CODE
#
# Example value:
# @CODE
# python_targets_python2_6(-)?,python_targets_python2_7(-)?
# @CODE

_python_set_globals() {
	local impls=()

	PYTHON_DEPS=
	local i PYTHON_PKG_DEP
	for i in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${i}" || continue

		python_export "${i}" PYTHON_PKG_DEP
		PYTHON_DEPS+="python_targets_${i}? ( ${PYTHON_PKG_DEP} ) "

		impls+=( "${i}" )
	done

	if [[ ${#impls[@]} -eq 0 ]]; then
		die "No supported implementation in PYTHON_COMPAT."
	fi

	local flags=( "${impls[@]/#/python_targets_}" )
	local optflags=${flags[@]/%/(-)?}

	# A nice QA trick here. Since a python-single-r1 package has to have
	# at least one PYTHON_SINGLE_TARGET enabled (REQUIRED_USE),
	# the following check will always fail on those packages. Therefore,
	# it should prevent developers from mistakenly depending on packages
	# not supporting multiple Python implementations.

	local flags_st=( "${impls[@]/#/-python_single_target_}" )
	optflags+=,${flags_st[@]/%/(-)}

	IUSE=${flags[*]}
	#REQUIRED_USE="|| ( ${flags[*]} )"
	PYTHON_USEDEP=${optflags// /,}

	# 1) well, python-exec would suffice as an RDEP
	# but no point in making this overcomplex, BDEP doesn't hurt anyone
	# 2) python-exec should be built with all targets forced anyway
	# but if new targets were added, we may need to force a rebuild
	PYTHON_DEPS+="dev-python/python-exec[${PYTHON_USEDEP}]"
}
_python_set_globals

# @FUNCTION: _python_validate_useflags
# @INTERNAL
# @DESCRIPTION:
# Enforce the proper setting of PYTHON_TARGETS.
_python_validate_useflags() {
	debug-print-function ${FUNCNAME} "${@}"

	local i

	for i in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${i}" || continue

		use "python_targets_${i}" && return 0
	done

	eerror "No Python implementation selected for the build. Please add one"
	eerror "of the following values to your PYTHON_TARGETS (in make.conf):"
	eerror
	eerror "${PYTHON_COMPAT[@]}"
	echo
	die "No supported Python implementation in PYTHON_TARGETS."
}

# @FUNCTION: python_gen_usedep
# @USAGE: <pattern> [...]
# @DESCRIPTION:
# Output a USE dependency string for Python implementations which
# are both in PYTHON_COMPAT and match any of the patterns passed
# as parameters to the function.
#
# When all implementations are requested, please use ${PYTHON_USEDEP}
# instead. Please also remember to set an appropriate REQUIRED_USE
# to avoid ineffective USE flags.
#
# Example:
# @CODE
# PYTHON_COMPAT=( python{2_7,3_2} )
# DEPEND="doc? ( dev-python/epydoc[$(python_gen_usedep python2*)] )"
# @CODE
#
# It will cause the dependency to look like:
# @CODE
# DEPEND="doc? ( dev-python/epydoc[python_targets_python2_7?] )"
# @CODE
python_gen_usedep() {
	debug-print-function ${FUNCNAME} "${@}"

	local impl pattern
	local matches=()

	for impl in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${impl}" || continue

		for pattern; do
			if [[ ${impl} == ${pattern} ]]; then
				matches+=(
					"python_targets_${impl}(-)?"
					"-python_single_target_${impl}(-)"
				)
				break
			fi
		done
	done

	local out=${matches[@]}
	echo ${out// /,}
}

# @FUNCTION: python_gen_useflags
# @USAGE: <pattern> [...]
# @DESCRIPTION:
# Output a list of USE flags for Python implementations which
# are both in PYTHON_COMPAT and match any of the patterns passed
# as parameters to the function.
#
# Example:
# @CODE
# PYTHON_COMPAT=( python{2_7,3_2} )
# REQUIRED_USE="doc? ( || ( $(python_gen_useflags python2*) ) )"
# @CODE
#
# It will cause the variable to look like:
# @CODE
# REQUIRED_USE="doc? ( || ( python_targets_python2_7 ) )"
# @CODE
python_gen_useflags() {
	debug-print-function ${FUNCNAME} "${@}"

	local impl pattern
	local matches=()

	for impl in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${impl}" || continue

		for pattern; do
			if [[ ${impl} == ${pattern} ]]; then
				matches+=( "python_targets_${impl}" )
				break
			fi
		done
	done

	echo ${matches[@]}
}

# @FUNCTION: python_gen_cond_dep
# @USAGE: <dependency> <pattern> [...]
# @DESCRIPTION:
# Output a list of <dependency>-ies made conditional to USE flags
# of Python implementations which are both in PYTHON_COMPAT and match
# any of the patterns passed as the remaining parameters.
#
# Please note that USE constraints on the package need to be enforced
# separately. Therefore, the dependency usually needs to use
# python_gen_usedep as well.
#
# Example:
# @CODE
# PYTHON_COMPAT=( python{2_5,2_6,2_7} )
# RDEPEND="$(python_gen_cond_dep dev-python/unittest2 python{2_5,2_6})"
# @CODE
#
# It will cause the variable to look like:
# @CODE
# RDEPEND="python_targets_python2_5? ( dev-python/unittest2 )
#	python_targets_python2_6? ( dev-python/unittest2 )"
# @CODE
python_gen_cond_dep() {
	debug-print-function ${FUNCNAME} "${@}"

	local impl pattern
	local matches=()

	local dep=${1}
	shift

	for impl in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${impl}" || continue

		for pattern; do
			if [[ ${impl} == ${pattern} ]]; then
				matches+=( "python_targets_${impl}? ( ${dep} )" )
				break
			fi
		done
	done

	echo ${matches[@]}
}

# @ECLASS-VARIABLE: BUILD_DIR
# @DESCRIPTION:
# The current build directory. In global scope, it is supposed to
# contain an initial build directory; if unset, it defaults to ${S}.
#
# In functions run by python_foreach_impl(), the BUILD_DIR is locally
# set to an implementation-specific build directory. That path is
# created through appending a hyphen and the implementation name
# to the final component of the initial BUILD_DIR.
#
# Example value:
# @CODE
# ${WORKDIR}/foo-1.3-python2_6
# @CODE

# @FUNCTION: python_copy_sources
# @DESCRIPTION:
# Create a single copy of the package sources (${S}) for each enabled
# Python implementation.
#
# The sources are always copied from S to implementation-specific build
# directories respecting BUILD_DIR.
python_copy_sources() {
	debug-print-function ${FUNCNAME} "${@}"

	_python_validate_useflags

	local impl
	local bdir=${BUILD_DIR:-${S}}

	debug-print "${FUNCNAME}: bdir = ${bdir}"
	einfo "Will copy sources from ${S}"
	# the order is irrelevant here
	for impl in "${PYTHON_COMPAT[@]}"; do
		_python_impl_supported "${impl}" || continue

		if use "python_targets_${impl}"
		then
			local BUILD_DIR=${bdir%%/}-${impl}

			einfo "${impl}: copying to ${BUILD_DIR}"
			debug-print "${FUNCNAME}: [${impl}] cp ${S} => ${BUILD_DIR}"
			cp -pr "${S}" "${BUILD_DIR}" || die
		fi
	done
}

# @FUNCTION: _python_check_USE_PYTHON
# @INTERNAL
# @DESCRIPTION:
# Check whether USE_PYTHON and PYTHON_TARGETS are in sync. Output
# warnings if they are not.
_python_check_USE_PYTHON() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ ! ${_PYTHON_USE_PYTHON_CHECKED} ]]; then
		_PYTHON_USE_PYTHON_CHECKED=1

		# python-exec has profile-forced flags.
		if [[ ${CATEGORY}/${PN} == dev-python/python-exec ]]; then
			return
		fi

		_try_eselect() {
			# The eselect solution will work only with one py2 & py3.

			local impl py2 py3 dis_py2 dis_py3
			for impl in "${PYTHON_COMPAT[@]}"; do
				_python_impl_supported "${impl}" || continue

				if use "python_targets_${impl}"; then
					case "${impl}" in
						python2_*)
							if [[ ${py2+1} ]]; then
								debug-print "${FUNCNAME}: -> more than one py2: ${py2} ${impl}"
								return 1
							fi
							py2=${impl/_/.}
							;;
						python3_*)
							if [[ ${py3+1} ]]; then
								debug-print "${FUNCNAME}: -> more than one py3: ${py3} ${impl}"
								return 1
							fi
							py3=${impl/_/.}
							;;
						*)
							return 1
							;;
					esac
				else
					case "${impl}" in
						python2_*)
							dis_py2=1
							;;
						python3_*)
							dis_py3=1
							;;
					esac
				fi
			done

			# The eselect solution won't work if the disabled Python version
			# is installed.
			if [[ ! ${py2+1} && ${dis_py2} ]]; then
				debug-print "${FUNCNAME}: -> all py2 versions disabled"
				if ! has python2_7 "${PYTHON_COMPAT[@]}"; then
					debug-print "${FUNCNAME}: ---> package does not support 2.7"
					return 0
				fi
				if has_version '=dev-lang/python-2*'; then
					debug-print "${FUNCNAME}: ---> but =python-2* installed!"
					return 1
				fi
			fi
			if [[ ! ${py3+1} && ${dis_py3} ]]; then
				debug-print "${FUNCNAME}: -> all py3 versions disabled"
				if ! has python3_2 "${PYTHON_COMPAT[@]}"; then
					debug-print "${FUNCNAME}: ---> package does not support 3.2"
					return 0
				fi
				if has_version '=dev-lang/python-3*'; then
					debug-print "${FUNCNAME}: ---> but =python-3* installed!"
					return 1
				fi
			fi

			local warned

			# Now check whether the correct implementations are active.
			if [[ ${py2+1} ]]; then
				local sel_py2=$(eselect python show --python2)

				debug-print "${FUNCNAME}: -> py2 built: ${py2}, active: ${sel_py2}"
				if [[ ${py2} != ${sel_py2} ]]; then
					ewarn "Building package for ${py2} only while ${sel_py2} is active."
					ewarn "Please consider switching the active Python 2 interpreter:"
					ewarn
					ewarn "	eselect python set --python2 ${py2}"
					warned=1
				fi
			fi

			if [[ ${py3+1} ]]; then
				local sel_py3=$(eselect python show --python3)

				debug-print "${FUNCNAME}: -> py3 built: ${py3}, active: ${sel_py3}"
				if [[ ${py3} != ${sel_py3} ]]; then
					[[ ${warned} ]] && ewarn
					ewarn "Building package for ${py3} only while ${sel_py3} is active."
					ewarn "Please consider switching the active Python 3 interpreter:"
					ewarn
					ewarn "	eselect python set --python3 ${py3}"
					warned=1
				fi
			fi

			if [[ ${warned} ]]; then
				ewarn
				ewarn "Please note that after switching the active Python interpreter,"
				ewarn "you may need to run 'python-updater' to rebuild affected packages."
				ewarn
				ewarn "For more information on python.eclass compatibility, please see"
				ewarn "the appropriate python-r1 User's Guide chapter [1]."
				ewarn
				ewarn "[1] http://www.gentoo.org/proj/en/Python/python-r1/user-guide.xml#doc_chap2"
			fi
		}

		# If user has no USE_PYTHON, try to avoid it.
		if [[ ! ${USE_PYTHON} ]]; then
			debug-print "${FUNCNAME}: trying eselect solution ..."
			_try_eselect && return
		fi

		debug-print "${FUNCNAME}: trying USE_PYTHON solution ..."
		debug-print "${FUNCNAME}: -> USE_PYTHON=${USE_PYTHON}"

		local impl old=${USE_PYTHON} new=() removed=()

		for impl in "${PYTHON_COMPAT[@]}"; do
			_python_impl_supported "${impl}" || continue

			local abi
			case "${impl}" in
				python*)
					abi=${impl#python}
					;;
				jython*)
					abi=${impl#jython}-jython
					;;
				pypy*)
					abi=2.7-pypy-${impl#pypy}
					;;
				*)
					die "Unexpected Python implementation: ${impl}"
					;;
			esac
			abi=${abi/_/.}

			has "${abi}" ${USE_PYTHON}
			local has_abi=${?}
			use "python_targets_${impl}"
			local has_impl=${?}

			# 0 = has, 1 = does not have
			if [[ ${has_abi} == 0 && ${has_impl} == 1 ]]; then
				debug-print "${FUNCNAME}: ---> remove ${abi}"
				# remove from USE_PYTHON
				old=${old/${abi}/}
				removed+=( ${abi} )
			elif [[ ${has_abi} == 1 && ${has_impl} == 0 ]]; then
				debug-print "${FUNCNAME}: ---> add ${abi}"
				# add to USE_PYTHON
				new+=( ${abi} )
			fi
		done

		if [[ ${removed[@]} || ${new[@]} ]]; then
			old=( ${old} )

			debug-print "${FUNCNAME}: -> old: ${old[@]}"
			debug-print "${FUNCNAME}: -> new: ${new[@]}"
			debug-print "${FUNCNAME}: -> removed: ${removed[@]}"

			if [[ ${USE_PYTHON} ]]; then
				ewarn "It seems that your USE_PYTHON setting lists different Python"
				ewarn "implementations than your PYTHON_TARGETS variable. Please consider"
				ewarn "using the following value instead:"
				ewarn
				ewarn "	USE_PYTHON='\033[35m${old[@]}${new[@]+ \033[1m${new[@]}}\033[0m'"

				if [[ ${removed[@]} ]]; then
					ewarn
					ewarn "(removed \033[31m${removed[@]}\033[0m)"
				fi
			else
				ewarn "It seems that you need to set USE_PYTHON to make sure that legacy"
				ewarn "packages will be built with respect to PYTHON_TARGETS correctly:"
				ewarn
				ewarn "	USE_PYTHON='\033[35;1m${new[@]}\033[0m'"
			fi

			ewarn
			ewarn "Please note that after changing the USE_PYTHON variable, you may need"
			ewarn "to run 'python-updater' to rebuild affected packages."
			ewarn
			ewarn "For more information on python.eclass compatibility, please see"
			ewarn "the appropriate python-r1 User's Guide chapter [1]."
			ewarn
			ewarn "[1] http://www.gentoo.org/proj/en/Python/python-r1/user-guide.xml#doc_chap2"
		fi
	fi
}

# @FUNCTION: python_foreach_impl
# @USAGE: <command> [<args>...]
# @DESCRIPTION:
# Run the given command for each of the enabled Python implementations.
# If additional parameters are passed, they will be passed through
# to the command. If the command fails, python_foreach_impl dies.
# If necessary, use ':' to force a successful return.
#
# For each command being run, EPYTHON, PYTHON and BUILD_DIR are set
# locally, and the former two are exported to the command environment.
python_foreach_impl() {
	debug-print-function ${FUNCNAME} "${@}"

	_python_validate_useflags
	_python_check_USE_PYTHON

	local impl
	local bdir=${BUILD_DIR:-${S}}

	debug-print "${FUNCNAME}: bdir = ${bdir}"
	for impl in "${_PYTHON_ALL_IMPLS[@]}"; do
		if has "${impl}" "${PYTHON_COMPAT[@]}" \
			&& _python_impl_supported "${impl}" \
			&& use "python_targets_${impl}"
		then
			local EPYTHON PYTHON
			python_export "${impl}" EPYTHON PYTHON
			local BUILD_DIR=${bdir%%/}-${impl}
			export EPYTHON PYTHON

			einfo "${EPYTHON}: running ${@}"
			"${@}" || die "${EPYTHON}: ${1} failed"
		fi
	done
}

# @FUNCTION: python_export_best
# @USAGE: [<variable>...]
# @DESCRIPTION:
# Find the best (most preferred) Python implementation enabled
# and export given variables for it. If no variables are provided,
# EPYTHON & PYTHON will be exported.
python_export_best() {
	debug-print-function ${FUNCNAME} "${@}"

	_python_validate_useflags

	[[ ${#} -gt 0 ]] || set -- EPYTHON PYTHON

	local impl best
	for impl in "${_PYTHON_ALL_IMPLS[@]}"; do
		if has "${impl}" "${PYTHON_COMPAT[@]}" \
			&& _python_impl_supported "${impl}" \
			&& use "python_targets_${impl}"
		then
			best=${impl}
		fi
	done

	[[ ${best+1} ]] || die "python_export_best(): no implementation found!"

	debug-print "${FUNCNAME}: Best implementation is: ${impl}"
	python_export "${impl}" "${@}"
}

# @FUNCTION: python_replicate_script
# @USAGE: <path>...
# @DESCRIPTION:
# Copy the given script to variants for all enabled Python
# implementations, then replace it with a symlink to the wrapper.
#
# All specified files must start with a 'python' shebang. A file not
# having a matching shebang will be refused.
python_replicate_script() {
	debug-print-function ${FUNCNAME} "${@}"

	_python_validate_useflags

	local suffixes=()

	_add_suffix() {
		suffixes+=( "${EPYTHON}" )
	}
	python_foreach_impl _add_suffix
	debug-print "${FUNCNAME}: suffixes = ( ${suffixes[@]} )"

	local f suffix
	for suffix in "${suffixes[@]}"; do
		for f; do
			local newf=${f}-${suffix}

			debug-print "${FUNCNAME}: ${f} -> ${newf}"
			cp "${f}" "${newf}" || die
		done

		_python_rewrite_shebang "${suffix}" "${@/%/-${suffix}}"
	done

	for f; do
		_python_ln_rel "${ED}"/usr/bin/python-exec "${f}" || die
	done
}

# @FUNCTION: run_in_build_dir
# @USAGE: <argv>...
# @DESCRIPTION:
# Run the given command in the directory pointed by BUILD_DIR.
run_in_build_dir() {
	debug-print-function ${FUNCNAME} "${@}"
	local ret

	[[ ${#} -ne 0 ]] || die "${FUNCNAME}: no command specified."
	[[ ${BUILD_DIR} ]] || die "${FUNCNAME}: BUILD_DIR not set."

	pushd "${BUILD_DIR}" >/dev/null || die
	"${@}"
	ret=${?}
	popd >/dev/null || die

	return ${ret}
}

_PYTHON_R1=1
fi
