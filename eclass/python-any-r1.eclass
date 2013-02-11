# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/python-any-r1.eclass,v 1.6 2013/01/21 19:28:16 mgorny Exp $

# @ECLASS: python-any-r1
# @MAINTAINER:
# Python team <python@gentoo.org>
# @AUTHOR:
# Author: Michał Górny <mgorny@gentoo.org>
# Based on work of: Krzysztof Pawlik <nelchael@gentoo.org>
# @BLURB: An eclass for packages having build-time dependency on Python.
# @DESCRIPTION:
# A minimal eclass for packages which need any Python interpreter
# installed without a need for explicit choice and invariability.
# This usually involves packages requiring Python at build-time
# but having no other relevance to it.
#
# This eclass provides a minimal PYTHON_DEPS variable with a dependency
# string on any of the supported Python implementations. It also exports
# pkg_setup() which finds the best supported implementation and sets it
# as the active one.
#
# Please note that python-any-r1 will always inherit python-utils-r1
# as well. Thus, all the functions defined there can be used in the
# packages using python-any-r1, and there is no need ever to inherit
# both.
#
# For more information, please see the python-r1 Developer's Guide:
# http://www.gentoo.org/proj/en/Python/python-r1/dev-guide.xml

case "${EAPI:-0}" in
	0|1|2|3|4|5)
		# EAPI=4 needed by python-r1
		;;
	*)
		die "Unsupported EAPI=${EAPI} (unknown) for ${ECLASS}"
		;;
esac

if [[ ! ${_PYTHON_ANY_R1} ]]; then

if [[ ${_PYTHON_R1} ]]; then
	die 'python-any-r1.eclass can not be used with python-r1.eclass.'
elif [[ ${_PYTHON_SINGLE_R1} ]]; then
	die 'python-any-r1.eclass can not be used with python-single-r1.eclass.'
fi

inherit python-utils-r1

fi

EXPORT_FUNCTIONS pkg_setup

if [[ ! ${_PYTHON_ANY_R1} ]]; then

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
# The list of USEflags required to be enabled on the Python
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
# || ( dev-lang/python:X.Y[gdbm,ncurses(-)?] ... )
# @CODE

# @ECLASS-VARIABLE: PYTHON_DEPS
# @DESCRIPTION:
# This is an eclass-generated Python dependency string for all
# implementations listed in PYTHON_COMPAT.
#
# Any of the supported interpreters will satisfy the dependency.
#
# Example use:
# @CODE
# DEPEND="${RDEPEND}
#	${PYTHON_DEPS}"
# @CODE
#
# Example value:
# @CODE
# || ( dev-lang/python:2.7[gdbm]
# 	dev-lang/python:2.6[gdbm] )
# @CODE

_python_build_set_globals() {
	local usestr
	[[ ${PYTHON_REQ_USE} ]] && usestr="[${PYTHON_REQ_USE}]"

	PYTHON_DEPS=
	local i
	for i in "${_PYTHON_ALL_IMPLS[@]}"; do
		if has "${i}" "${PYTHON_COMPAT[@]}"
		then
			local d
			case ${i} in
				python*)
					d='dev-lang/python';;
				jython*)
					d='dev-java/jython';;
				pypy*)
					d='dev-python/pypy';;
				*)
					die "Invalid implementation: ${i}"
			esac

			local v=${i##*[a-z]}
			PYTHON_DEPS="${d}:${v/_/.}${usestr} ${PYTHON_DEPS}"
		fi
	done
	PYTHON_DEPS="|| ( ${PYTHON_DEPS})"
}
_python_build_set_globals

# @FUNCTION: _python_EPYTHON_supported
# @USAGE: <epython>
# @INTERNAL
# @DESCRIPTION:
# Check whether the specified implementation is supported by package
# (specified in PYTHON_COMPAT).
_python_EPYTHON_supported() {
	debug-print-function ${FUNCNAME} "${@}"

	local i=${1/./_}

	case "${i}" in
		python*|jython*)
			;;
		pypy-c*)
			i=${i/-c/}
			;;
		*)
			ewarn "Invalid EPYTHON: ${EPYTHON}"
			return 1
			;;
	esac

	if has "${i}" "${PYTHON_COMPAT[@]}"; then
		return 0
	elif ! has "${i}" "${_PYTHON_ALL_IMPLS[@]}"; then
		ewarn "Invalid EPYTHON: ${EPYTHON}"
	fi
	return 1
}

# @FUNCTION: python-any-r1_pkg_setup
# @DESCRIPTION:
# Determine what the best installed (and supported) Python
# implementation is and set EPYTHON and PYTHON accordingly.
python-any-r1_pkg_setup() {
	debug-print-function ${FUNCNAME} "${@}"

	# first, try ${EPYTHON}... maybe it's good enough for us.
	if [[ ${EPYTHON} ]]; then
		if _python_EPYTHON_supported "${EPYTHON}"; then
			python_export EPYTHON PYTHON
			return
		fi
	fi

	# then, try eselect-python
	local variant i
	for variant in '' '--python2' '--python3'; do
		i=$(eselect python --show ${variant} 2>/dev/null)

		if [[ ! ${i} ]]; then
			# no eselect-python?
			break
		elif _python_EPYTHON_supported "${i}"; then
			python_export "${i}" EPYTHON PYTHON
			return
		fi
	done

	# fallback to best installed impl.
	local rev_impls=()
	for i in "${_PYTHON_ALL_IMPLS[@]}"; do
		if has "${i}" "${PYTHON_COMPAT[@]}"; then
			rev_impls=( "${i}" "${rev_impls[@]}" )
		fi
	done

	local PYTHON_PKG_DEP
	for i in "${rev_impls[@]}"; do
		python_export "${i}" PYTHON_PKG_DEP EPYTHON PYTHON
		ROOT=/ has_version "${PYTHON_PKG_DEP}" && return
	done
}

_PYTHON_ANY_R1=1
fi
