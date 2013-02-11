# Copyright 1999-2013 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/eclass/distutils-r1.eclass,v 1.48 2013/01/27 16:39:23 mgorny Exp $

# @ECLASS: distutils-r1
# @MAINTAINER:
# Python team <python@gentoo.org>
# @AUTHOR:
# Author: Michał Górny <mgorny@gentoo.org>
# Based on the work of: Krzysztof Pawlik <nelchael@gentoo.org>
# @BLURB: A simple eclass to build Python packages using distutils.
# @DESCRIPTION:
# A simple eclass providing functions to build Python packages using
# the distutils build system. It exports phase functions for all
# the src_* phases. Each of the phases runs two pseudo-phases:
# python_..._all() (e.g. python_prepare_all()) once in ${S}, then
# python_...() (e.g. python_prepare()) for each implementation
# (see: python_foreach_impl() in python-r1).
#
# In distutils-r1_src_prepare(), the 'all' function is run before
# per-implementation ones (because it creates the implementations),
# per-implementation functions are run in a random order.
#
# In remaining phase functions, the per-implementation functions are run
# before the 'all' one, and they are ordered from the least to the most
# preferred implementation (so that 'better' files overwrite 'worse'
# ones).
#
# If the ebuild doesn't specify a particular pseudo-phase function,
# the default one will be used (distutils-r1_...). Defaults are provided
# for all per-implementation pseudo-phases, python_prepare_all()
# and python_install_all(); whenever writing your own pseudo-phase
# functions, you should consider calling the defaults (and especially
# distutils-r1_python_prepare_all).
#
# Please note that distutils-r1 sets RDEPEND and DEPEND unconditionally
# for you.
#
# Also, please note that distutils-r1 will always inherit python-r1
# as well. Thus, all the variables defined and documented there are
# relevant to the packages using distutils-r1.
#
# For more information, please see the python-r1 Developer's Guide:
# http://www.gentoo.org/proj/en/Python/python-r1/dev-guide.xml

case "${EAPI:-0}" in
	0|1|2|3)
		die "Unsupported EAPI=${EAPI:-0} (too old) for ${ECLASS}"
		;;
	4|5)
		;;
	*)
		die "Unsupported EAPI=${EAPI} (unknown) for ${ECLASS}"
		;;
esac

# @ECLASS-VARIABLE: DISTUTILS_OPTIONAL
# @DEFAULT_UNSET
# @DESCRIPTION:
# If set to a non-null value, distutils part in the ebuild will
# be considered optional. No dependencies will be added and no phase
# functions will be exported.
#
# If you enable DISTUTILS_OPTIONAL, you have to set proper dependencies
# for your package (using ${PYTHON_DEPS}) and to either call
# distutils-r1 default phase functions or call the build system
# manually.

# @ECLASS-VARIABLE: DISTUTILS_SINGLE_IMPL
# @DEFAULT_UNSET
# @DESCRIPTION:
# If set to a non-null value, the ebuild will support setting a single
# Python implementation only. It will effectively replace the python-r1
# eclass inherit with python-single-r1.
#
# Note that inheriting python-single-r1 will cause pkg_setup()
# to be exported. It must be run in order for the eclass functions
# to function properly.

if [[ ! ${_DISTUTILS_R1} ]]; then

inherit eutils

if [[ ! ${DISTUTILS_SINGLE_IMPL} ]]; then
	inherit multiprocessing python-r1
else
	inherit python-single-r1
fi

fi

if [[ ! ${DISTUTILS_OPTIONAL} ]]; then
	EXPORT_FUNCTIONS src_prepare src_configure src_compile src_test src_install
fi

if [[ ! ${_DISTUTILS_R1} ]]; then

if [[ ! ${DISTUTILS_OPTIONAL} ]]; then
	RDEPEND=${PYTHON_DEPS}
	DEPEND=${PYTHON_DEPS}
fi

# @ECLASS-VARIABLE: DISTUTILS_JOBS
# @DEFAULT_UNSET
# @DESCRIPTION:
# The number of parallel jobs to run for distutils-r1 parallel builds.
# If unset, the job-count in ${MAKEOPTS} will be used.
#
# This variable is intended to be set in make.conf.

# @ECLASS-VARIABLE: PATCHES
# @DEFAULT_UNSET
# @DESCRIPTION:
# An array containing patches to be applied to the sources before
# copying them.
#
# If unset, no custom patches will be applied.
#
# Please note, however, that at some point the eclass may apply
# additional distutils patches/quirks independently of this variable.
#
# Example:
# @CODE
# PATCHES=( "${FILESDIR}"/${P}-make-gentoo-happy.patch )
# @CODE

# @ECLASS-VARIABLE: DOCS
# @DEFAULT_UNSET
# @DESCRIPTION:
# An array containing documents installed using dodoc. The files listed
# there must exist in the directory from which
# distutils-r1_python_install_all() is run (${S} by default).
#
# If unset, the function will instead look up files matching default
# filename pattern list (from the Package Manager Specification),
# and install those found.
#
# Example:
# @CODE
# DOCS=( NEWS README )
# @CODE

# @ECLASS-VARIABLE: HTML_DOCS
# @DEFAULT_UNSET
# @DESCRIPTION:
# An array containing documents installed using dohtml. The files
# and directories listed there must exist in the directory from which
# distutils-r1_python_install_all() is run (${S} by default).
#
# If unset, no HTML docs will be installed.
#
# Example:
# @CODE
# HTML_DOCS=( doc/html/ )
# @CODE

# @ECLASS-VARIABLE: DISTUTILS_IN_SOURCE_BUILD
# @DEFAULT_UNSET
# @DESCRIPTION:
# If set to a non-null value, in-source builds will be enabled.
# If unset, the default is to use in-source builds when python_prepare()
# is declared, and out-of-source builds otherwise.
#
# If in-source builds are used, the eclass will create a copy of package
# sources for each Python implementation in python_prepare_all(),
# and work on that copy afterwards.
#
# If out-of-source builds are used, the eclass will instead work
# on the sources directly, prepending setup.py arguments with
# 'build --build-base ${BUILD_DIR}' to enforce keeping & using built
# files in the specific root.

# @ECLASS-VARIABLE: DISTUTILS_NO_PARALLEL_BUILD
# @DEFAULT_UNSET
# @DESCRIPTION:
# If set to a non-null value, the parallel build feature will
# be disabled.
#
# When parallel builds are used, the implementation-specific sub-phases
# for selected Python implementation will be run in parallel. This will
# increase build efficiency with distutils which does not do parallel
# builds.
#
# This variable can be used to disable the afore-mentioned feature
# in case it causes issues with the package.

# @ECLASS-VARIABLE: mydistutilsargs
# @DEFAULT_UNSET
# @DESCRIPTION:
# An array containing options to be passed to setup.py.
#
# Example:
# @CODE
# python_configure_all() {
# 	mydistutilsargs=( --enable-my-hidden-option )
# }
# @CODE

# @FUNCTION: esetup.py
# @USAGE: [<args>...]
# @DESCRIPTION:
# Run the setup.py using currently selected Python interpreter
# (if ${PYTHON} is set; fallback 'python' otherwise). The setup.py will
# be passed default ${mydistutilsargs[@]}, then any parameters passed
# to this command and optionally a standard option set (e.g. the build
# directory in an ebuild using out-of-source builds).
#
# This command dies on failure.
esetup.py() {
	debug-print-function ${FUNCNAME} "${@}"

	local add_args=()
	if [[ ! ${DISTUTILS_IN_SOURCE_BUILD} ]]; then
		if [[ ! ${BUILD_DIR} ]]; then
			die 'Out-of-source build requested, yet BUILD_DIR unset.'
		fi

		add_args+=(
			build
			--build-base "${BUILD_DIR}"
			# using a single directory for them helps us export ${PYTHONPATH}
			--build-lib "${BUILD_DIR}/lib"
			# make the ebuild writer lives easier
			--build-scripts "${BUILD_DIR}/scripts"
		)
	fi

	set -- "${PYTHON:-python}" setup.py \
		"${mydistutilsargs[@]}" "${@}" "${add_args[@]}"

	echo "${@}" >&2
	"${@}" || die
}

# @FUNCTION: distutils-r1_python_prepare_all
# @DESCRIPTION:
# The default python_prepare_all(). It applies the patches from PATCHES
# array, then user patches and finally calls python_copy_sources to
# create copies of resulting sources for each Python implementation.
#
# At some point in the future, it may also apply eclass-specific
# distutils patches and/or quirks.
distutils-r1_python_prepare_all() {
	debug-print-function ${FUNCNAME} "${@}"

	[[ ${PATCHES} ]] && epatch "${PATCHES[@]}"

	epatch_user

	# by default, use in-source build if python_prepare() is used
	if [[ ! ${DISTUTILS_IN_SOURCE_BUILD+1} ]]; then
		if declare -f python_prepare >/dev/null; then
			DISTUTILS_IN_SOURCE_BUILD=1
		fi
	fi

	if [[ ${DISTUTILS_IN_SOURCE_BUILD} && ! ${DISTUTILS_SINGLE_IMPL} ]]
	then
		# create source copies for each implementation
		python_copy_sources
	fi
}

# @FUNCTION: distutils-r1_python_prepare
# @DESCRIPTION:
# The default python_prepare(). A no-op.
distutils-r1_python_prepare() {
	debug-print-function ${FUNCNAME} "${@}"

	:
}

# @FUNCTION: distutils-r1_python_configure
# @DESCRIPTION:
# The default python_configure(). A no-op.
distutils-r1_python_configure() {
	debug-print-function ${FUNCNAME} "${@}"

	:
}

# @FUNCTION: distutils-r1_python_compile
# @USAGE: [additional-args...]
# @DESCRIPTION:
# The default python_compile(). Runs 'esetup.py build'. Any parameters
# passed to this function will be appended to setup.py invocation,
# i.e. passed as options to the 'build' command.
distutils-r1_python_compile() {
	debug-print-function ${FUNCNAME} "${@}"

	esetup.py build "${@}"
}

# @FUNCTION: distutils-r1_python_test
# @DESCRIPTION:
# The default python_test(). A no-op.
distutils-r1_python_test() {
	debug-print-function ${FUNCNAME} "${@}"

	:
}

# @FUNCTION: _distutils-r1_rename_scripts
# @USAGE: <path>
# @INTERNAL
# @DESCRIPTION:
# Renames installed Python scripts to be implementation-suffixed.
# ${EPYTHON} needs to be set to the implementation name.
#
# All executable scripts having shebang referencing ${EPYTHON}
# in given path will be renamed.
_distutils-r1_rename_scripts() {
	debug-print-function ${FUNCNAME} "${@}"

	local path=${1}
	[[ ${path} ]] || die "${FUNCNAME}: no path given"

	local f
	while IFS= read -r -d '' f; do
		debug-print "${FUNCNAME}: found executable at ${f#${D}/}"

		if [[ "$(head -n 1 "${f}")" == '#!'*${EPYTHON}* ]]
		then
			debug-print "${FUNCNAME}: matching shebang: $(head -n 1 "${f}")"

			local newf=${f}-${EPYTHON}
			debug-print "${FUNCNAME}: renaming to ${newf#${D}/}"
			mv "${f}" "${newf}" || die

			debug-print "${FUNCNAME}: installing wrapper at ${f#${D}/}"
			_python_ln_rel "${path}${EPREFIX}"/usr/bin/python-exec "${f}" || die
		fi
	done < <(find "${path}" -type f -executable -print0)
}

# @FUNCTION: distutils-r1_python_install
# @USAGE: [additional-args...]
# @DESCRIPTION:
# The default python_install(). Runs 'esetup.py install', appending
# the optimization flags. Then renames the installed scripts.
# Any parameters passed to this function will be appended
# to the setup.py invocation (i.e. as options to the 'install' command).
distutils-r1_python_install() {
	debug-print-function ${FUNCNAME} "${@}"

	local flags

	case "${EPYTHON}" in
		jython*)
			flags=(--compile);;
		*)
			flags=(--compile -O2);;
	esac
	debug-print "${FUNCNAME}: [${EPYTHON}] flags: ${flags}"

	# enable compilation for the install phase.
	local PYTHONDONTWRITEBYTECODE
	export PYTHONDONTWRITEBYTECODE

	# python likes to compile any module it sees, which triggers sandbox
	# failures if some packages haven't compiled their modules yet.
	addpredict "$(python_get_sitedir)"
	addpredict /usr/lib/portage/pym

	local root=${D}/_${EPYTHON}
	[[ ${DISTUTILS_SINGLE_IMPL} ]] && root=${D}

	esetup.py install "${flags[@]}" --root="${root}" "${@}"

	if [[ ! ${DISTUTILS_SINGLE_IMPL} ]]; then
		_distutils-r1_rename_scripts "${root}"
		_distutils-r1_merge_root "${root}" "${D}"
	fi
}

# @FUNCTION: distutils-r1_merge_root
# @USAGE: <src-root> <dest-root>
# @INTERNAL
# @DESCRIPTION:
# Merge the directory tree from <src-root> to <dest-root>, removing
# the <src-root> in the process.
_distutils-r1_merge_root() {
	local src=${1}
	local dest=${2}

	local lockfile=${T}/distutils-r1-merge-lock

	if type -P lockf &>/dev/null; then
		# On BSD, we have 'lockf' wrapper.
		tar -C "${src}" -f - -c . \
			| lockf "${lockfile}" tar -x -f - -C "${dest}"
	else
		local lock_fd
		if type -P flock &>/dev/null; then
			# On Linux, we have 'flock' which can lock fd.
			redirect_alloc_fd lock_fd "${lockfile}" '>>'
			flock ${lock_fd}
		else
			ewarn "distutils-r1: no locking service found, please report."
		fi

		cp -a -l -n "${src}"/. "${dest}"/

		if [[ ${lock_fd} ]]; then
			# Close the lock file when we are done with it.
			# Prevents deadlock if we aren't in a subshell.
			eval "exec ${lock_fd}>&-"
		fi
	fi
	[[ ${?} == 0 ]] || die "Merging ${EPYTHON} image failed."

	rm -rf "${src}"
}

# @FUNCTION: distutils-r1_python_install_all
# @DESCRIPTION:
# The default python_install_all(). It installs the documentation.
distutils-r1_python_install_all() {
	debug-print-function ${FUNCNAME} "${@}"

	if declare -p DOCS &>/dev/null; then
		dodoc -r "${DOCS[@]}" || die "dodoc failed"
	else
		local f
		# same list as in PMS
		for f in README* ChangeLog AUTHORS NEWS TODO CHANGES \
				THANKS BUGS FAQ CREDITS CHANGELOG; do
			if [[ -s ${f} ]]; then
				dodoc "${f}" || die "(default) dodoc ${f} failed"
			fi
		done
	fi

	if declare -p HTML_DOCS &>/dev/null; then
		dohtml -r "${HTML_DOCS[@]}" || die "dohtml failed"
	fi
}

# @FUNCTION: distutils-r1_run_phase
# @USAGE: [<argv>...]
# @INTERNAL
# @DESCRIPTION:
# Run the given command.
#
# If out-of-source builds are used, the phase function is run in source
# directory, with BUILD_DIR pointing at the build directory
# and PYTHONPATH having an entry for the module build directory.
#
# If in-source builds are used, the command is executed in the BUILD_DIR
# (the directory holding per-implementation copy of sources).
distutils-r1_run_phase() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ ${DISTUTILS_IN_SOURCE_BUILD} ]]; then
		if [[ ! ${DISTUTILS_SINGLE_IMPL} ]]; then
			pushd "${BUILD_DIR}" >/dev/null || die
		fi
	else
		local PYTHONPATH="${BUILD_DIR}/lib:${PYTHONPATH}"
		export PYTHONPATH
	fi

	local TMPDIR=${T}/${EPYTHON}

	mkdir -p "${TMPDIR}" || die

	if [[ ${DISTUTILS_NO_PARALLEL_BUILD} || ${DISTUTILS_SINGLE_IMPL} ]]
	then
		"${@}" 2>&1 | tee -a "${T}/build-${EPYTHON}.log"
	else
		(
			multijob_child_init
			"${@}" 2>&1 | tee -a "${T}/build-${EPYTHON}.log"
		) &
		multijob_post_fork
	fi

	if [[ ${DISTUTILS_IN_SOURCE_BUILD} && ! ${DISTUTILS_SINGLE_IMPL} ]]
	then
		popd >/dev/null || die
	fi

	# Store them for reuse.
	_DISTUTILS_BEST_IMPL=(
		"${EPYTHON}" "${PYTHON}" "${BUILD_DIR}" "${PYTHONPATH}"
	)
}

# @FUNCTION: _distutils-r1_run_common_phase
# @USAGE: [<argv>...]
# @INTERNAL
# @DESCRIPTION:
# Run the given command, restoring the best-implementation state.
_distutils-r1_run_common_phase() {
	local DISTUTILS_ORIG_BUILD_DIR=${BUILD_DIR}

	local EPYTHON=${_DISTUTILS_BEST_IMPL[0]}
	local PYTHON=${_DISTUTILS_BEST_IMPL[1]}
	local BUILD_DIR=${_DISTUTILS_BEST_IMPL[2]}
	local PYTHONPATH=${_DISTUTILS_BEST_IMPL[3]}

	export EPYTHON PYTHON PYTHONPATH

	einfo "common: running ${1}"
	"${@}"
}

# @FUNCTION: _distutils-r1_multijob_init
# @INTERNAL
# @DESCRIPTION:
# Init multijob, taking the job-count from ${DISTUTILS_JOBS}.
_distutils-r1_multijob_init() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ ! ${DISTUTILS_NO_PARALLEL_BUILD} && ! ${DISTUTILS_SINGLE_IMPL} ]]
	then
		local opts
		if [[ ${DISTUTILS_JOBS} ]]; then
			opts=-j${DISTUTILS_JOBS}
		else
			opts=${MAKEOPTS}
		fi

		multijob_init "${opts}"
	fi
}

# @FUNCTION: _distutils-r1_multijob_finish
# @INTERNAL
# @DESCRIPTION:
# Finish multijob if used.
_distutils-r1_multijob_finish() {
	debug-print-function ${FUNCNAME} "${@}"

	if [[ ! ${DISTUTILS_NO_PARALLEL_BUILD} && ! ${DISTUTILS_SINGLE_IMPL} ]]
	then
		multijob_finish
	fi
}

# @FUNCTION: _distutils-r1_run_foreach_impl
# @INTERNAL
# @DESCRIPTION:
# Run the given phase for each implementation if multiple implementations
# are enabled, once otherwise.
_distutils-r1_run_foreach_impl() {
	debug-print-function ${FUNCNAME} "${@}"

	set -- distutils-r1_run_phase "${@}"

	if [[ ! ${DISTUTILS_SINGLE_IMPL} ]]; then
		python_foreach_impl "${@}"
	else
		if [[ ! ${EPYTHON} ]]; then
			die "EPYTHON unset, python-single-r1_pkg_setup not called?!"
		fi
		local BUILD_DIR=${BUILD_DIR:-${S}}
		BUILD_DIR=${BUILD_DIR%%/}_${EPYTHON}

		"${@}"
	fi
}

distutils-r1_src_prepare() {
	debug-print-function ${FUNCNAME} "${@}"

	# common preparations
	if declare -f python_prepare_all >/dev/null; then
		python_prepare_all
	else
		distutils-r1_python_prepare_all
	fi

	_distutils-r1_multijob_init
	if declare -f python_prepare >/dev/null; then
		_distutils-r1_run_foreach_impl python_prepare
	fi
	_distutils-r1_multijob_finish
}

distutils-r1_src_configure() {
	_distutils-r1_multijob_init
	if declare -f python_configure >/dev/null; then
		_distutils-r1_run_foreach_impl python_configure
	fi
	_distutils-r1_multijob_finish

	if declare -f python_configure_all >/dev/null; then
		_distutils-r1_run_common_phase python_configure_all
	fi
}

distutils-r1_src_compile() {
	debug-print-function ${FUNCNAME} "${@}"

	_distutils-r1_multijob_init
	if declare -f python_compile >/dev/null; then
		_distutils-r1_run_foreach_impl python_compile
	else
		_distutils-r1_run_foreach_impl distutils-r1_python_compile
	fi
	_distutils-r1_multijob_finish

	if declare -f python_compile_all >/dev/null; then
		_distutils-r1_run_common_phase python_compile_all
	fi
}

distutils-r1_src_test() {
	debug-print-function ${FUNCNAME} "${@}"

	_distutils-r1_multijob_init
	if declare -f python_test >/dev/null; then
		_distutils-r1_run_foreach_impl python_test
	fi
	_distutils-r1_multijob_finish

	if declare -f python_test_all >/dev/null; then
		_distutils-r1_run_common_phase python_test_all
	fi
}

distutils-r1_src_install() {
	debug-print-function ${FUNCNAME} "${@}"

	_distutils-r1_multijob_init
	if declare -f python_install >/dev/null; then
		_distutils-r1_run_foreach_impl python_install
	else
		_distutils-r1_run_foreach_impl distutils-r1_python_install
	fi
	_distutils-r1_multijob_finish

	if declare -f python_install_all >/dev/null; then
		_distutils-r1_run_common_phase python_install_all
	else
		_distutils-r1_run_common_phase distutils-r1_python_install_all
	fi
}

_DISTUTILS_R1=1
fi
