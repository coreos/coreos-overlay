# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

#
# Original Author: The Chromium OS Authors <chromium-os-dev@chromium.org>
# Purpose: Eclass for handling autotest test packages
#

RDEPEND="( autotest? ( >=chromeos-base/autotest-0.0.1-r3 ) )"

IUSE="+buildcheck autotest opengles"

# Ensure the configures run by autotest pick up the right config.site
export CONFIG_SITE="/usr/share/config.site"
export AUTOTEST_WORKDIR="${WORKDIR}/autotest-work"

# @ECLASS-VARIABLE: AUTOTEST_CLIENT_*
# @DESCRIPTION:
# Location of the appropriate test directory inside ${S}
: ${AUTOTEST_CLIENT_TESTS:=client/tests}
: ${AUTOTEST_CLIENT_SITE_TESTS:=client/site_tests}
: ${AUTOTEST_CONFIG:=client/config}
: ${AUTOTEST_DEPS:=client/deps}
: ${AUTOTEST_PROFILERS:=client/profilers}

# @ECLASS-VARIABLE: AUTOTEST_*_LIST
# @DESCRIPTION:
# The list of deps/configs/profilers provided with this package
: ${AUTOTEST_CONFIG_LIST:=}
: ${AUTOTEST_DEPS_LIST:=}
: ${AUTOTEST_PROFILERS_LIST:=}

# @ECLASS-VARIABLE: AUTOTEST_FORCE_LIST
# @DESCRIPTION:
# Sometimes we just want to forget about useflags and build what's inside
: ${AUTOTEST_FORCE_TEST_LIST:=}

# @ECLASS-VARIABLE: AUTOTEST_FILE_MASK
# @DESCRIPTION:
# The list of 'find' expressions to find in the resulting image and delete
: ${AUTOTEST_FILE_MASK:=}

fast_cp() {
	cp -l "$@" || cp "$@"
}

get_test_list() {
	if [ -n "${AUTOTEST_FORCE_TEST_LIST}" ]; then
		# list forced
		echo "${AUTOTEST_FORCE_TEST_LIST}"
		return
	fi

	# we cache the result of this operation in AUTOTEST_TESTS,
	# because it's expensive and does not change over the course of one ebuild run
	local result="${IUSE_TESTS[*]//[+-]tests_/}"
	result="${result//tests_/}"

	result=$(for test in ${result}; do use tests_${test} && echo -n "${test} "; done)
	echo "${result}"
}

# Pythonify the list of packages by doing the equivalent of ','.join(args)
pythonify_test_list() {
	local result=$(printf '%s,' "$@")
	echo ${result%,}
}

# Create python package init files for top level test case dirs.
touch_init_py() {
	local dirs=${1}
	for base_dir in $dirs
	do
		local sub_dirs="$(find ${base_dir} -maxdepth 1 -type d)"
		for sub_dir in ${sub_dirs}
		do
			touch ${sub_dir}/__init__.py
		done
		touch ${base_dir}/__init__.py
	done
}

# Exports a CROS_WORKON_SUBDIRS_TO_COPY variable to ensure that only the
# necessary files will be copied. This cannot be applied globally, as some
# ebuilds may not have tests only.
autotest_restrict_workon_subdirs() {
	CROS_WORKON_SUBDIRS_TO_COPY=()
	local var
	for var in AUTOTEST_{CLIENT,SERVER}_{TESTS,SITE_TESTS} \
	           AUTOTEST_{CONFIG,DEPS,PROFILERS}; do
		CROS_WORKON_SUBDIRS_TO_COPY+=( ${!var} )
	done
}

setup_cross_toolchain() {
	tc-export CC CXX AR RANLIB LD NM STRIP PKG_CONFIG
	export CCFLAGS="$CFLAGS"

	# TODO(fes): Check for /etc/hardened for now instead of the hardened
	# use flag because we aren't enabling hardened on the target board.
	# Rather, right now we're using hardened only during toolchain compile.
	# Various tests/etc. use %ebx in here, so we have to turn off PIE when
	# using the hardened compiler
	if use x86 ; then
		if use hardened ; then
			#CC="${CC} -nopie"
			append-flags -nopie
		fi
	fi
}

create_autotest_workdir() {
	local dst=${1}

	# create a working enviroment for pre-building
	ln -sf "${SYSROOT}"/usr/local/autotest/{conmux,tko,global_config.ini,shadow_config.ini} "${dst}"/

	# NOTE: in order to make autotest not notice it's running from /usr/local/, we need
	# to make sure the binaries are real, because they do the path magic
	local root_path base_path
	for base_path in utils server; do
		root_path="${SYSROOT}/usr/local/autotest/${base_path}"
		mkdir -p "${dst}/${base_path}"
		for entry in $(ls "${root_path}"); do
			# Take all important binaries from SYSROOT install, make a copy.
			if [ -d "${root_path}/${entry}" ]; then
				# Ignore anything that has already been put in place by
				# something else. This will typically be server/{site_tests,tests}.
				if ! [ -e "${dst}/${base_path}/${entry}" ]; then
					ln -sf "${root_path}/${entry}" "${dst}/${base_path}/"
				fi
			else
				cp -f ${root_path}/${entry} ${dst}/${base_path}/
			fi
		done
	done
	for base_path in client client/bin; do
		root_path="${SYSROOT}/usr/local/autotest/${base_path}"
		mkdir -p "${dst}/${base_path}"

		# Skip bin, because it is processed separately, and test-provided dirs
		# Also don't symlink to packages, because that kills the build
		for entry in $(ls "${root_path}" | \
			grep -v "\(bin\|tests\|site_tests\|config\|deps\|profilers\|packages\)$"); do
			ln -sf "${root_path}/${entry}" "${dst}/${base_path}/"
		done
	done
	# replace the important binaries with real copies
	for base_path in autotest autotest_client; do
		root_path="${SYSROOT}/usr/local/autotest/client/bin/${base_path}"
		rm "${dst}/client/bin/${base_path}"
		cp -f ${root_path} "${dst}/client/bin/${base_path}"
	done

	# Selectively pull in deps that are not provided by the current test package
	for base_path in config deps profilers; do
		for dir in "${SYSROOT}/usr/local/autotest/client/${base_path}"/*; do
			if [ -d "${dir}" ] && \
				! [ -d "${AUTOTEST_WORKDIR}/client/${base_path}/$(basename ${dir})" ]; then
				# directory does not exist, create a symlink
				ln -sf "${dir}" "${AUTOTEST_WORKDIR}/client/${base_path}/$(basename ${dir})"
			fi
		done
	done
}

print_test_dirs() {
	local testroot="${1}"
	local ignore_test_contents="${2}"

	pushd "${testroot}" 1> /dev/null
	for test in *; do
		if [ -d "${test}" ] && [ -n "${ignore_test_contents}" -o \
					-f "${test}/${test}".py ]; then
			echo "${test}"
		fi
	done
	popd 1> /dev/null
}

# checks IUSE_TESTS and sees if at least one of these is enabled
are_we_used() {
	if ! use autotest; then
		# unused
		return 1
	fi

	[ -n "$(get_test_list)" ] && return 0

	# unused
	return 1
}

autotest_src_prepare() {
	are_we_used || return 0
	einfo "Preparing tests"

	# FIXME: These directories are needed, autotest quietly dies if
	# they don't even exist. They may, however, stay empty.
	mkdir -p "${AUTOTEST_WORKDIR}"/client/tests
	mkdir -p "${AUTOTEST_WORKDIR}"/client/site_tests
	mkdir -p "${AUTOTEST_WORKDIR}"/client/config
	mkdir -p "${AUTOTEST_WORKDIR}"/client/deps
	mkdir -p "${AUTOTEST_WORKDIR}"/client/profilers

	TEST_LIST=$(get_test_list)

	# Pull in the individual test cases.
	for l1 in client server; do
	for l2 in site_tests tests; do
		# pick up the indicated location of test sources
		eval srcdir=${WORKDIR}/${P}/\${AUTOTEST_${l1^^*}_${l2^^*}}

		# test does have this directory
		for test in ${TEST_LIST}; do
			if [ -d "${srcdir}/${test}" ]; then
				fast_cp -fpr "${srcdir}/${test}" "${AUTOTEST_WORKDIR}/${l1}/${l2}"/ || die
			fi
		done
	done
	done

	# Pull in all the deps provided by this package, selectively.
	for l2 in config deps profilers; do
		# pick up the indicated location of test sources
		eval srcdir=${WORKDIR}/${P}/\${AUTOTEST_${l2^^*}}

		if [ -d "${srcdir}" ]; then # test does have this directory
			pushd "${srcdir}" 1> /dev/null
			eval deplist=\${AUTOTEST_${l2^^*}_LIST}

			if [ "${deplist}" = "*" ]; then
				fast_cp -fpr * "${AUTOTEST_WORKDIR}/client/${l2}"
			else
				for dir in ${deplist}; do
					fast_cp -fpr "${dir}" "${AUTOTEST_WORKDIR}/client/${l2}"/ || die
				done
			fi
			popd 1> /dev/null
		fi
	done

	# FIXME: We'd like if this were not necessary, and autotest supported out-of-tree build
	create_autotest_workdir "${AUTOTEST_WORKDIR}"

	# Each test directory needs to be visited and have an __init__.py created.
	# However, that only applies to the directories which have a main .py file.
	pushd "${AUTOTEST_WORKDIR}" > /dev/null || die "AUTOTEST_WORKDIR does not exist?!"
	for dir in client/tests client/site_tests; do
		pushd "${dir}" > /dev/null || continue
		for sub in *; do
			[ -f "${sub}/${sub}.py" ] || continue

			touch_init_py ${sub}
		done
		popd > /dev/null
	done
	popd > /dev/null

	# Cleanup checked-in binaries that don't support the target architecture
	[[ ${E_MACHINE} == "" ]] && return 0;
	rm -fv $( scanelf -RmyBF%a "${AUTOTEST_WORKDIR}" | grep -v -e ^${E_MACHINE} )
}

autotest_src_compile() {
	if ! are_we_used; then
		ewarn "***************************************************************"
		ewarn "* Not building any tests, because the requested list is empty *"
		ewarn "***************************************************************"
		return 0
	fi
	einfo "Compiling tests"

	pushd "${AUTOTEST_WORKDIR}" 1> /dev/null

	setup_cross_toolchain

	if use opengles ; then
		graphics_backend=OPENGLES
	else
		graphics_backend=OPENGL
	fi

	# HACK: Some of the autotests depend on SYSROOT being defined, and die
	# a gruesome death if it isn't. But SYSROOT does not need to exist, for
	# example on the host, it doesn't. Let's define a compatible variable
	# here in case we have none.
	export SYSROOT=${SYSROOT:-"/"}

	# This only prints the tests that have the associated .py
	# (and therefore a setup function)
	local prebuild_test_dirs="
		client/tests client/site_tests"
	TESTS=$(\
		for dir in ${prebuild_test_dirs}; do
			print_test_dirs "${AUTOTEST_WORKDIR}/${dir}"
		done | sort -u)
	NR_TESTS=$(echo ${TESTS}|wc -w)
	if ! [ "${NR_TESTS}" = "0" ]; then
		einfo "Building tests (${NR_TESTS}):"
		einfo "${TESTS}"

		NORMAL=$(echo -e "\e[0m")
		GREEN=$(echo -e "\e[1;32m")
		RED=$(echo -e "\e[1;31m")

		# Call autotest to prebuild all test cases.
		# Parse output through a colorifying sed script
		( GRAPHICS_BACKEND="$graphics_backend" LOGNAME=${SUDO_USER} \
				client/bin/autotest_client --quiet \
			--client_test_setup=$(pythonify_test_list ${TESTS}) \
			|| ! use buildcheck || die "Tests failed to build."
		) | sed -e "s/\(INFO:root:setup\)/${GREEN}* \1${NORMAL}/" \
			-e "s/\(ERROR:root:\[.*\]\)/${RED}\1${NORMAL}/"
	else
		einfo "No tests to prebuild, skipping"
	fi

	# Cleanup some temp files after compiling
	for mask in '*.[do]' '*.pyc' ${AUTOTEST_FILE_MASK}; do
		einfo "Purging ${mask}"
		find . -name "${mask}" -delete
	done

	popd 1> /dev/null
}

autotest_src_install() {
	are_we_used || return 0
	einfo "Installing tests"

	# Install all test cases, after setup has been called on them.
	# We install everything, because nothing else is copied into the
	# testcase directories besides what this package provides.
	local instdirs="
		client/tests
		client/site_tests"

	for dir in ${instdirs}; do
		[ -d "${AUTOTEST_WORKDIR}/${dir}" ] || continue

		insinto /usr/local/autotest/$(dirname ${dir})
		doins -r "${AUTOTEST_WORKDIR}/${dir}"
	done

	# Install the deps, configs, profilers.
	# Difference from above is, we don't install the whole thing, just
	# the stuff provided by this package, by looking at AUTOTEST_*_LIST.
	instdirs="
		config
		deps
		profilers"

	for dir in ${instdirs}; do
		[ -d "${AUTOTEST_WORKDIR}/client/${dir}" ] || continue

		insinto /usr/local/autotest/client/${dir}

		eval provided=\${AUTOTEST_${dir^^*}_LIST}
		# * means provided all, figure out the list from ${S}
		if [ "${provided}" = "*" ]; then
			if eval pushd "${WORKDIR}/${P}/\${AUTOTEST_${dir^^*}}" &> /dev/null; then
				provided=$(ls)
				popd 1> /dev/null
			else
				provided=""
			fi
		fi

		for item in ${provided}; do
			doins -r "${AUTOTEST_WORKDIR}/client/${dir}/${item}"
		done
	done

	# TODO: Not all needs to be executable, but it's hard to pick selectively.
	# The source repo should already contain stuff with the right permissions.
	chmod -R a+x "${D}"/usr/local/autotest/*
}

autotest_pkg_postinst() {
	are_we_used || return 0
	local root_autotest_dir="${ROOT}/usr/local/autotest"
	local path_to_image="${D}/usr/local/autotest"
	# Should only happen when running emerge on a DUT.
	if [ ! -d "${root_autotest_dir}" ]; then
		einfo "Skipping packaging as no autotest installation detected."
		return 0
	fi

	# Gather the artifacts we want autotest to package.
	local test_opt dep_opt prof_opt

	# Only client tests can be packaged.
	local tests_to_package_dirs="client/tests client/site_tests"
	local client_tests=$(\
		for dir in ${tests_to_package_dirs}; do
			print_test_dirs "${path_to_image}/${dir}" yes
		done | sort -u)

	if [ -n "${client_tests}" ] && [ "${client_tests}" != "myfaketest" ]; then
		# Check for test_count. The packager performs poorly when
		# too many arguments are specified vs. --all. This should be fixed in
		# autotest (crosbug.com/28173).
		test_count=$(echo ${client_tests} | wc -w)
		if [ ${test_count} -gt 10 ]; then
			test_opt="--all"
		else
			test_opt="--test=$(pythonify_test_list ${client_tests})"
		fi
	fi

	if [ "{test_opt}" != "--all" ]; then
		if [ -n "${AUTOTEST_DEPS_LIST}" ]; then
			dep_opt="--dep=$(pythonify_test_list ${AUTOTEST_DEPS_LIST})"
		fi

		# For *, we must generate the list of profilers.
		if [ "${AUTOTEST_PROFILERS_LIST}" = "*" ]; then
			AUTOTEST_PROFILERS_LIST=$(\
				print_test_dirs "${path_to_image}/client/profilers" yes | sort -u)
				prof_opt="--profiler=$(pythonify_test_list ${AUTOTEST_PROFILERS_LIST})"
		fi
	fi

	if [ -n "${test_opt}" -o -n "${dep_opt}" -o -n "${prof_opt}" ]; then
		einfo "Running packager with options ${test_opt} ${dep_opt} ${prof_opt}"
		flock "${root_autotest_dir}/packages" \
			-c "python -B ${root_autotest_dir}/utils/packager.py \
				-r ${root_autotest_dir}/packages \
				${test_opt} ${dep_opt} ${prof_opt} upload"
	else
		einfo "Packager not run as nothing was found to package."
	fi
}

if [[ "${CROS_WORKON_PROJECT}" == "chromiumos/third_party/autotest" ]]; then
	# Using main autotest repo. Automatically restricting checkout.
	# Note that this does not happen if the inherit is done prior to setting
	# CROS_WORKON_* variables.
	autotest_restrict_workon_subdirs
fi

EXPORT_FUNCTIONS src_compile src_prepare src_install pkg_postinst
