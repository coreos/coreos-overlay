# Base eclass for Java packages
#
# Copyright (c) 2004-2005, Thomas Matthijs <axxo@gentoo.org>
# Copyright (c) 2004, Karl Trygve Kalleberg <karltk@gentoo.org>
# Copyright (c) 2004-2011, Gentoo Foundation
#
# Licensed under the GNU General Public License, v2
#
# $Header: /var/cvsroot/gentoo-x86/eclass/java-utils-2.eclass,v 1.152 2013/01/16 19:06:15 sera Exp $

# -----------------------------------------------------------------------------
# @eclass-begin
# @eclass-shortdesc Java Utility eclass
# @eclass-maintainer java@gentoo.org
#
# This eclass provides functionality which is used by
# java-pkg.eclass and java-pkg-opt.eclass as well as from ebuilds.
#
# @warning
#   You probably don't want to inherit this directly from an ebuild. Instead,
#   you should inherit java-ant for Ant-based Java packages, java-pkg for other
#   Java packages, or java-pkg-opt for packages that have optional Java support.
#
# -----------------------------------------------------------------------------

inherit eutils versionator multilib

IUSE="elibc_FreeBSD"

# -----------------------------------------------------------------------------
# @section-begin variables
# @section-title Variables
#
# Summary of variables which control the behavior of building Java packges.
# -----------------------------------------------------------------------------

# Make sure we use java-config-2
export WANT_JAVA_CONFIG="2"

# -----------------------------------------------------------------------------
# @variable-external WANT_ANT_TASKS
# @variable-default ""
#
# An $IFS separated list of ant tasks.
# Ebuild can specify this variable before inheriting java-ant-2 eclass to
# determine ANT_TASKS it needs. They will be automatically translated to
# DEPEND variable and ANT_TASKS variable. JAVA_PKG_FORCE_ANT_TASKS can override
# ANT_TASKS set by WANT_ANT_TASKS, but not the DEPEND due to caching.
# Ebuilds that need to depend conditionally on certain tasks and specify them
# differently for different eant calls can't use this simplified approach.
# You also cannot specify version or anything else than ant-*.
#
# @example WANT_ANT_TASKS="ant-junit ant-trax"
#
# @seealso JAVA_PKG_FORCE_ANT_TASKS
# -----------------------------------------------------------------------------
#WANT_ANT_TASKS

# -----------------------------------------------------------------------------
# @variable-internal JAVA_PKG_PORTAGE_DEP
#
# The version of portage we need to function properly. Previously it was
# portage with phase hooks support but now we use a version with proper env
# saving. For EAPI 2 we have new enough stuff so let's have cleaner deps.
# -----------------------------------------------------------------------------
has "${EAPI}" 0 1 && JAVA_PKG_PORTAGE_DEP=">=sys-apps/portage-2.1.2.7"

# -----------------------------------------------------------------------------
# @variable-internal JAVA_PKG_E_DEPEND
#
# This is a convience variable to be used from the other java eclasses. This is
# the version of java-config we want to use. Usually the latest stable version
# so that ebuilds can use new features without depending on specific versions.
# -----------------------------------------------------------------------------
JAVA_PKG_E_DEPEND=">=dev-java/java-config-2.1.9-r1 ${JAVA_PKG_PORTAGE_DEP}"
has source ${JAVA_PKG_IUSE} && JAVA_PKG_E_DEPEND="${JAVA_PKG_E_DEPEND} source? ( app-arch/zip )"

# -----------------------------------------------------------------------------
# @variable-preinherit JAVA_PKG_WANT_BOOTCLASSPATH
#
# The version of bootclasspath the package needs to work. Translates to a proper
# dependency. The bootclasspath has to be obtained by java-ant_rewrite-bootclasspath
# -----------------------------------------------------------------------------

if [[ -n "${JAVA_PKG_WANT_BOOTCLASSPATH}" ]]; then
	if [[ "${JAVA_PKG_WANT_BOOTCLASSPATH}" == "1.5" ]]; then
		JAVA_PKG_E_DEPEND="${JAVA_PKG_E_DEPEND} >=dev-java/gnu-classpath-0.98-r1:0.98"
	else
		eerror "Unknown value of JAVA_PKG_WANT_BOOTCLASSPATH"
		# since die in global scope doesn't work, this will make repoman fail
		JAVA_PKG_E_DEPEND="${JAVA_PKG_E_DEPEND} BAD_JAVA_PKG_WANT_BOOTCLASSPATH"
	fi
fi

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_ALLOW_VM_CHANGE
# @variable-default yes
#
# Allow this eclass to change the active VM?
# If your system VM isn't sufficient for the package, the build will fail.
# @note This is useful for testing specific VMs.
# -----------------------------------------------------------------------------
JAVA_PKG_ALLOW_VM_CHANGE=${JAVA_PKG_ALLOW_VM_CHANGE:="yes"}

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_FORCE_VM
#
# Explicitly set a particular VM to use. If its not valid, it'll fall back to
# whatever /etc/java-config-2/build/jdk.conf would elect to use.
#
# Should only be used for testing and debugging.
#
# @example Use sun-jdk-1.5 to emerge foo
#	JAVA_PKG_FORCE_VM=sun-jdk-1.5 emerge foo
#
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_WANT_BUILD_VM
#
# A list of VM handles to choose a build VM from. If the list contains the
# currently active VM use that one, otherwise step through the list till a
# usable/installed VM is found.
#
# This allows to use an explicit list of JDKs in DEPEND instead of a virtual.
# Users of this variable must make sure at least one of the listed handles is
# covered by DEPEND.
# Requires JAVA_PKG_WANT_SOURCE and JAVA_PKG_WANT_TARGET to be set as well. 
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_WANT_SOURCE
#
# Specify a specific VM version to compile for to use for -source.
# Normally this is determined from DEPEND.
# See java-pkg_get-source function below.
#
# Should only be used for testing and debugging.
#
# @seealso java-pkg_get-source
#
# @example Use 1.4 source to emerge baz
#	JAVA_PKG_WANT_SOURCE=1.4 emerge baz
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_WANT_TARGET
#
# Same as JAVA_PKG_WANT_SOURCE above but for -target.
# See java-pkg_get-target function below.
#
# Should only be used for testing and debugging.
#
# @seealso java-pkg_get-target
#
# @example emerge bar to be compatible with 1.3
#	JAVA_PKG_WANT_TARGET=1.3 emerge bar
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-internal JAVA_PKG_COMPILER_DIR
# @default /usr/share/java-config-2/compiler
#
# Directory where compiler settings are saved, without trailing slash.
# Probably shouldn't touch this variable.
# -----------------------------------------------------------------------------
JAVA_PKG_COMPILER_DIR=${JAVA_PKG_COMPILER_DIR:="/usr/share/java-config-2/compiler"}


# -----------------------------------------------------------------------------
# @variable-internal JAVA_PKG_COMPILERS_CONF
# @variable-default /etc/java-config-2/build/compilers.conf
#
# Path to file containing information about which compiler to use.
# Can be overloaded, but it should be overloaded for testing.
# -----------------------------------------------------------------------------
JAVA_PKG_COMPILERS_CONF=${JAVA_PKG_COMPILERS_CONF:="/etc/java-config-2/build/compilers.conf"}

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_FORCE_COMPILER
#
# Explicitly set a list of compilers to use. This is normally read from
# JAVA_PKG_COMPILERS_CONF.
#
# @note This should only be used internally or for testing.
# @example Use jikes and javac, in that order
#	JAVA_PKG_FORCE_COMPILER="jikes javac"
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @variable-external JAVA_PKG_FORCE_ANT_TASKS
#
# An $IFS separated list of ant tasks. Can be set in environment before calling
# emerge/ebuild to override variables set in ebuild, mainly for testing before
# putting the resulting (WANT_)ANT_TASKS into ebuild. Affects only ANT_TASKS in
# eant() call, not the dependencies specified in WANT_ANT_TASKS.
#
# @example JAVA_PKG_FORCE_ANT_TASKS="ant-junit ant-trax" \
# 	ebuild foo.ebuild compile
#
# @seealso WANT_ANT_TASKS
# -----------------------------------------------------------------------------

# TODO document me
JAVA_PKG_QA_VIOLATIONS=0

# -----------------------------------------------------------------------------
# @section-end variables
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# @section-begin install
# @section-summary Install functions
#
# These are used to install Java-related things, such as jars, Javadocs, JNI
# libraries, etc.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @ebuild-function java-pkg_doexamples
#
# Installs given arguments to /usr/share/doc/${PF}/examples
# If you give it only one parameter and it is a directory it will install
# everything in that directory to the examples directory.
#
# @example
#	java-pkg_doexamples demo
#	java-pkg_doexamples demo/* examples/*
#
# @param --subdir - If the examples need a certain directory structure
# @param $* - list of files to install
# ------------------------------------------------------------------------------
java-pkg_doexamples() {
	debug-print-function ${FUNCNAME} $*

	[[ ${#} -lt 1 ]] && die "At least one argument needed"

	java-pkg_check-phase install

	local dest=/usr/share/doc/${PF}/examples
	if [[ ${1} == --subdir ]]; then
		local dest=${dest}/${2}
		dodir ${dest}
		shift 2
	fi

	if [[ ${#} = 1 && -d ${1} ]]; then
		( # dont want to pollute calling env
			insinto "${dest}"
			doins -r ${1}/*
		) || die "Installing examples failed"
	else
		( # dont want to pollute calling env
			insinto "${dest}"
			doins -r "$@"
		) || die "Installing examples failed"
	fi

	# Let's make a symlink to the directory we have everything else under
	dosym "${dest}" "${JAVA_PKG_SHAREPATH}/examples" || die
}

# -----------------------------------------------------------------------------
# @ebuild-function java-pkg_dojar
#
# Installs any number of jars.
# Jar's will be installed into /usr/share/${PN}(-${SLOT})/lib/ by default.
# You can use java-pkg_jarinto to change this path.
# You should never install a jar with a package version in the filename.
# Instead, use java-pkg_newjar defined below.
#
# @example
#	java-pkg_dojar dist/${PN}.jar dist/${PN}-core.jar
#
# @param $* - list of jars to install
# ------------------------------------------------------------------------------
java-pkg_dojar() {
	debug-print-function ${FUNCNAME} $*

	[[ ${#} -lt 1 ]] && die "At least one argument needed"

	java-pkg_check-phase install
	java-pkg_init_paths_

	# Create JARDEST if it doesn't exist
	dodir ${JAVA_PKG_JARDEST}

	local jar
	# for each jar
	for jar in "${@}"; do
		local jar_basename=$(basename "${jar}")

		java-pkg_check-versioned-jar ${jar_basename}

		# check if it exists
		if [[ -e "${jar}" ]] ; then
			# Don't overwrite if jar has already been installed with the same
			# name
			local dest="${D}${JAVA_PKG_JARDEST}/${jar_basename}"
			if [[ -e "${dest}" ]]; then
				ewarn "Overwriting ${dest}"
			fi

			# install it into JARDEST if it's a non-symlink
			if [[ ! -L "${jar}" ]] ; then
				#but first check class version when in strict mode.
				is-java-strict && java-pkg_verify-classes "${jar}"

				INSDESTTREE="${JAVA_PKG_JARDEST}" \
					doins "${jar}" || die "failed to install ${jar}"
				java-pkg_append_ JAVA_PKG_CLASSPATH "${JAVA_PKG_JARDEST}/${jar_basename}"
				debug-print "installed ${jar} to ${D}${JAVA_PKG_JARDEST}"
			# make a symlink to the original jar if it's symlink
			else
				# TODO use dosym, once we find something that could use it
				# -nichoj
				ln -s "$(readlink "${jar}")" "${D}${JAVA_PKG_JARDEST}/${jar_basename}"
				debug-print "${jar} is a symlink, linking accordingly"
			fi
		else
			die "${jar} does not exist"
		fi
	done

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @internal-function depend-java-query
#
# Wrapper for the depend-java-query binary to enable passing USE in env.
# Using env variables keeps this eclass working with java-config versions that
# do not handle use flags.
# ------------------------------------------------------------------------------

depend-java-query() {
	# Used to have a which call here but it caused endless loops for some people
	# that had some weird bashrc voodoo for which.
	USE="${USE}" /usr/bin/depend-java-query "${@}"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_regjar
#
# Records an already installed jar in the package.env
# This would mostly be used if the package has make or a custom script to
# install things.
#
# Example:
#	java-pkg_regjar ${D}/opt/foo/lib/foo.jar
#
# WARNING:
#   if you want to use shell expansion, you have to use ${D}/... as the for in
#   this function will not be able to expand the path, here's an example:
#
#   java-pkg_regjar /opt/my-java/lib/*.jar
#
#   will not work, because:
#    * the `for jar in "$@"` can't expand the path to jar file names, as they
#      don't exist yet
#    * all `if ...` inside for will fail - the file '/opt/my-java/lib/*.jar'
#      doesn't exist
#
#   you have to use it as:
#
#   java-pkg_regjar ${D}/opt/my-java/lib/*.jar
#
# @param $@ - jars to record
# ------------------------------------------------------------------------------
# TODO should we be making sure the jar is present on ${D} or wherever?
java-pkg_regjar() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} -lt 1 ]] && die "at least one argument needed"

	java-pkg_init_paths_

	local jar jar_dir jar_file
	for jar in "${@}"; do
		# TODO use java-pkg_check-versioned-jar
		if [[ -e "${jar}" || -e "${D}${jar}" ]]; then
			[[ -d "${jar}" || -d "${D}${jar}" ]] \
				&& die "Called ${FUNCNAME} on a	directory $*"

			#check that class version correct when in strict mode
			is-java-strict && java-pkg_verify-classes "${jar}"

			# nelchael: we should strip ${D} in this case too, here's why:
			# imagine such call:
			#    java-pkg_regjar ${D}/opt/java/*.jar
			# such call will fall into this case (-e ${jar}) and will
			# record paths with ${D} in package.env
			java-pkg_append_ JAVA_PKG_CLASSPATH	"${jar#${D}}"
		else
			if [[ ${jar} = *\** ]]; then
				eerror "The argument ${jar} to ${FUNCNAME}"
				eerror "has * in it. If you want it to glob in"
				eerror '${D} add ${D} to the argument.'
			fi
			debug-print "${jar} or ${D}${jar} not found"
			die "${jar} does not exist"
		fi
	done

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_newjar
#
# Installs a jar with a new name
#
# @example: install a versioned jar without the version
#	java-pkg_newjar dist/${P}.jar ${PN}.jar
#
# @param $1 - jar to install
# @param $2 - new name for jar - defaults to ${PN}.jar if not specified
# ------------------------------------------------------------------------------
java-pkg_newjar() {
	debug-print-function ${FUNCNAME} $*

	local original_jar="${1}"
	local new_jar="${2:-${PN}.jar}"
	local new_jar_dest="${T}/${new_jar}"

	[[ -z ${original_jar} ]] && die "Must specify a jar to install"
	[[ ! -f ${original_jar} ]] \
		&& die "${original_jar} does not exist or is not a file!"

	rm -f "${new_jar_dest}" || die "Failed to remove ${new_jar_dest}"
	cp "${original_jar}" "${new_jar_dest}" \
		|| die "Failed to copy ${original_jar} to ${new_jar_dest}"
	java-pkg_dojar "${new_jar_dest}"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_addcp
#
# Add something to the package's classpath. For jars, you should use dojar,
# newjar, or regjar. This is typically used to add directories to the classpath.
#
# TODO add example
# @param $@ - value to append to JAVA_PKG_CLASSPATH
# ------------------------------------------------------------------------------
java-pkg_addcp() {
	java-pkg_append_ JAVA_PKG_CLASSPATH "${@}"
	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_doso
#
# Installs any number of JNI libraries
# They will be installed into /usr/lib by default, but java-pkg_sointo
# can be used change this path
#
# Example:
#	java-pkg_doso *.so
#
# @param $@ - JNI libraries to install
# ------------------------------------------------------------------------------
java-pkg_doso() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} -lt 1 ]] && die "${FUNCNAME} requires at least one argument"

	java-pkg_init_paths_

	local lib
	# for each lib
	for lib in "$@" ; do
		# if the lib exists...
		if [[ -e "${lib}" ]] ; then
			# install if it isn't a symlink
			if [[ ! -L "${lib}" ]] ; then
				INSDESTTREE="${JAVA_PKG_LIBDEST}" \
					INSOPTIONS="${LIBOPTIONS}" \
					doins "${lib}" || die "failed to install ${lib}"
				java-pkg_append_ JAVA_PKG_LIBRARY "${JAVA_PKG_LIBDEST}"
				debug-print "Installing ${lib} to ${JAVA_PKG_LIBDEST}"
			# otherwise make a symlink to the symlink's origin
			else
				dosym "$(readlink "${lib}")" "${JAVA_PKG_LIBDEST}/${lib##*/}"
				debug-print "${lib} is a symlink, linking accordantly"
			fi
		# otherwise die
		else
			die "${lib} does not exist"
		fi
	done

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_regso
#
# Registers an already JNI library in package.env.
#
# Example:
#	java-pkg_regso *.so /path/*.so
#
# @param $@ - JNI libraries to register
# ------------------------------------------------------------------------------
java-pkg_regso() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} -lt 1 ]] && die "${FUNCNAME} requires at least one argument"

	java-pkg_init_paths_

	local lib target_dir
	for lib in "$@" ; do
		# Check the absolute path of the lib
		if [[ -e "${lib}" ]] ; then
			target_dir="$(java-pkg_expand_dir_ ${lib})"
			java-pkg_append_ JAVA_PKG_LIBRARY "/${target_dir#${D}}"
		# Check the path of the lib relative to ${D}
		elif [[ -e "${D}${lib}" ]]; then
			target_dir="$(java-pkg_expand_dir_ ${D}${lib})"
			java-pkg_append_ JAVA_PKG_LIBRARY "${target_dir}"
		else
			die "${lib} does not exist"
		fi
	done

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_jarinto
#
# Changes the path jars are installed into
#
# @param $1 - new location to install jars into.
# -----------------------------------------------------------------------------
java-pkg_jarinto() {
	debug-print-function ${FUNCNAME} $*

	JAVA_PKG_JARDEST="${1}"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_sointo
#
# Changes the path that JNI libraries are installed into.
#
# @param $1 - new location to install JNI libraries into.
# ------------------------------------------------------------------------------
java-pkg_sointo() {
	debug-print-function ${FUNCNAME} $*

	JAVA_PKG_LIBDEST="${1}"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_dohtml
#
# Install Javadoc HTML documentation
#
# @example
#	java-pkg_dohtml dist/docs/
#
# ------------------------------------------------------------------------------
java-pkg_dohtml() {
	debug-print-function ${FUNCNAME} $*

	[[ ${#} -lt 1 ]] &&  die "At least one argument required for ${FUNCNAME}"

	# from /usr/lib/portage/bin/dohtml -h
	#  -f   Set list of allowed extensionless file names.
	dohtml -f package-list "$@"

	# this probably shouldn't be here but it provides
	# a reasonable way to catch # docs for all of the
	# old ebuilds.
	java-pkg_recordjavadoc
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_dojavadoc
#
# Installs javadoc documentation. This should be controlled by the doc use flag.
#
# @param $1: optional --symlink creates to symlink like this for html
#            documentation bundles.
# @param $2: - The javadoc root directory.
#
# @example:
#	java-pkg_dojavadoc docs/api
#   java-pkg_dojavadoc --symlink apidocs docs/api
#
# ------------------------------------------------------------------------------
java-pkg_dojavadoc() {
	debug-print-function ${FUNCNAME} $*

	# For html documentation bundles that link to Javadoc
	local symlink
	if [[ ${1} = --symlink ]]; then
		symlink=${2}
		shift 2
	fi

	local dir="$1"
	local dest=/usr/share/doc/${PF}/html

	# QA checks

	java-pkg_check-phase install

	[[ -z "${dir}" ]] && die "Must specify a directory!"
	[[ ! -d "${dir}" ]] && die "${dir} does not exist, or isn't a directory!"
	if [[ ! -e "${dir}/index.html" ]]; then
		local msg="No index.html in javadoc directory"
		ewarn "${msg}"
		is-java-strict && die "${msg}"
	fi

	if [[ -e ${D}/${dest}/api ]]; then
		eerror "${dest} already exists. Will not overwrite."
		die "${dest}"
	fi

	# Renaming to match our directory layout

	local dir_to_install="${dir}"
	if [[ "$(basename "${dir}")" != "api" ]]; then
		dir_to_install="${T}/api"
		# TODO use doins
		cp -r "${dir}" "${dir_to_install}" || die "cp failed"
	fi

	# Actual installation

	java-pkg_dohtml -r "${dir_to_install}"

	# Let's make a symlink to the directory we have everything else under
	dosym ${dest}/api "${JAVA_PKG_SHAREPATH}/api" || die

	if [[ ${symlink} ]]; then
		debug-print "symlinking ${dest}/{api,${symlink}}"
		dosym ${dest}/{api,${symlink}} || die
	fi
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_dosrc
#
# Installs a zip containing the source for a package, so it can used in
# from IDEs like eclipse and netbeans.
#
# Ebuild needs to DEPEND on app-arch/zip to use this.
#
# It also should be controlled by USE=source.
#
# @example:
#	java-pkg_dosrc src/*
#
# ------------------------------------------------------------------------------
# TODO change so it the arguments it takes are the base directories containing
# 	source -nichoj
# TODO should we be able to handle multiple calls to dosrc? -nichoj
# TODO maybe we can take an existing zip/jar? -nichoj
# FIXME apparently this fails if you give it an empty directories
java-pkg_dosrc() {
	debug-print-function ${FUNCNAME} $*

	[ ${#} -lt 1 ] && die "At least one argument needed"

	java-pkg_check-phase install

	[[ ${#} -lt 1 ]] && die "At least one argument needed"

	if ! [[ ${DEPEND} = *app-arch/zip* ]]; then
		local msg="${FUNCNAME} called without app-arch/zip in DEPEND"
		java-pkg_announce-qa-violation ${msg}
	fi

	java-pkg_init_paths_

	local zip_name="${PN}-src.zip"
	local zip_path="${T}/${zip_name}"
	local dir
	for dir in "${@}"; do
		local dir_parent=$(dirname "${dir}")
		local dir_name=$(basename "${dir}")
		pushd ${dir_parent} > /dev/null || die "problem entering ${dir_parent}"
		zip -q -r ${zip_path} ${dir_name} -i '*.java'
		local result=$?
		# 12 means zip has nothing to do
		if [[ ${result} != 12 && ${result} != 0 ]]; then
			die "failed to zip ${dir_name}"
		fi
		popd >/dev/null
	done

	# Install the zip
	INSDESTTREE=${JAVA_PKG_SOURCESPATH} \
		doins ${zip_path} || die "Failed to install source"

	JAVA_SOURCES="${JAVA_PKG_SOURCESPATH}/${zip_name}"
	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_dolauncher
#
# Make a wrapper script to lauch/start this package
# If necessary, the wrapper will switch to the appropriate VM.
#
# Can be called without parameters if the package installs only one jar
# that has the Main-class attribute set. The wrapper will be named ${PN}.
#
# @param $1 - filename of launcher to create
# @param $2 - options, as follows:
#  --main the.main.class.too.start
#  --jar /the/jar/too/launch.jar or just <name>.jar
#  --java_args 'Extra arguments to pass to java'
#  --pkg_args 'Extra arguments to pass to the package'
#  --pwd Directory the launcher changes to before executing java
#  -into Directory to install the launcher to, instead of /usr/bin
#  -pre Prepend contents of this file to the launcher
# ------------------------------------------------------------------------------
java-pkg_dolauncher() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install
	java-pkg_init_paths_

	if [[ ${#} = 0 ]]; then
		local name="${PN}"
	else
		local name="${1}"
		shift
	fi

	# TODO rename to launcher
	local target="${T}/${name}"
	local var_tmp="${T}/launcher_variables_tmp"
	local target_dir pre

	# Process the other the rest of the arguments
	while [[ -n "${1}" && -n "${2}" ]]; do
		local var="${1}" value="${2}"
		if [[ "${var:0:2}" == "--" ]]; then
			local var=${var:2}
			echo "gjl_${var}=\"${value}\"" >> "${var_tmp}"
			local gjl_${var}="${value}"
		elif [[ "${var}" == "-into" ]]; then
			target_dir="${value}"
		elif [[ "${var}" == "-pre" ]]; then
			pre="${value}"
		fi
		shift 2
	done

	# Test if no --jar and --main arguments were given and
	# in that case check if the package only installs one jar
	# and use that jar.
	if [[ -z "${gjl_jar}" && -z "${gjl_main}" ]]; then
		local cp="${JAVA_PKG_CLASSPATH}"
		if [[ "${cp/:}" = "${cp}" && "${cp%.jar}" != "${cp}" ]]; then
			echo "gjl_jar=\"${JAVA_PKG_CLASSPATH}\"" >> "${var_tmp}"
		else
			local msg="Not enough information to create a launcher given."
			msg="${msg} Please give --jar or --main argument to ${FUNCNAME}."
			die "${msg}"
		fi
	fi

	# Write the actual script
	echo "#!/bin/bash" > "${target}"
	if [[ -n "${pre}" ]]; then
		if [[ -f "${pre}" ]]; then
			cat "${pre}" >> "${target}"
		else
			die "-pre specified file '${pre}' does not exist"
		fi
	fi
	echo "gjl_package=${JAVA_PKG_NAME}" >> "${target}"
	cat "${var_tmp}" >> "${target}"
	rm -f "${var_tmp}"
	echo "source /usr/share/java-config-2/launcher/launcher.bash" >> "${target}"

	if [[ -n "${target_dir}" ]]; then
		DESTTREE="${target_dir}" dobin "${target}"
		local ret=$?
		return ${ret}
	else
		dobin "${target}"
	fi
}

# ------------------------------------------------------------------------------
# Install war files.
# TODO document
# ------------------------------------------------------------------------------
java-pkg_dowar() {
	debug-print-function ${FUNCNAME} $*

	# Check for arguments
	[[ ${#} -lt 1 ]] && die "At least one argument needed"
	java-pkg_check-phase install

	java-pkg_init_paths_

	local war
	for war in $* ; do
		local warpath
		# TODO evaluate if we want to handle symlinks differently -nichoj
		# Check for symlink
		if [[ -L "${war}" ]] ; then
			cp "${war}" "${T}"
			warpath="${T}$(basename "${war}")"
		# Check for directory
		# TODO evaluate if we want to handle directories differently -nichoj
		elif [[ -d "${war}" ]] ; then
			echo "dowar: warning, skipping directory ${war}"
			continue
		else
			warpath="${war}"
		fi

		# Install those files like you mean it
		INSOPTIONS="-m 0644" \
			INSDESTTREE=${JAVA_PKG_WARDEST} \
			doins ${warpath}
	done
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_recordjavadoc
# Scan for JavaDocs, and record their existence in the package.env file
#
# TODO make sure this in the proper section
# ------------------------------------------------------------------------------
java-pkg_recordjavadoc()
{
	debug-print-function ${FUNCNAME} $*
	# the find statement is important
	# as some packages include multiple trees of javadoc
	JAVADOC_PATH="$(find ${D}/usr/share/doc/ -name allclasses-frame.html -printf '%h:')"
	# remove $D - TODO: check this is ok with all cases of the above
	JAVADOC_PATH="${JAVADOC_PATH//${D}}"
	if [[ -n "${JAVADOC_PATH}" ]] ; then
		debug-print "javadocs found in ${JAVADOC_PATH%:}"
		java-pkg_do_write_
	else
		debug-print "No javadocs found"
	fi
}

# ------------------------------------------------------------------------------
# @section-end install
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @begin-section query
# Use these to build the classpath for building a package.
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_jar-from
#
# Makes a symlink to a jar from a certain package
# A lot of java packages include dependencies in a lib/ directory
# You can use this function to replace these bundled dependencies.
# The dependency is recorded into package.env DEPEND line, unless "--build-only"
# is passed as the very first argument, for jars that have to be present only
# at build time and are not needed on runtime (junit testing etc).
#
# Example: get all jars from xerces slot 2
#	java-pkg_jar-from xerces-2
# Example: get a specific jar from xerces slot 2
# 	java-pkg_jar-from xerces-2 xml-apis.jar
# Example: get a specific jar from xerces slot 2, and name it diffrently
# 	java-pkg_jar-from xerces-2 xml-apis.jar xml.jar
# Example: get junit.jar which is needed only for building
#	java-pkg_jar-from --build-only junit junit.jar
#
# @param $opt
#	--build-only - makes the jar(s) not added into package.env DEPEND line.
#	  (assumed automatically when called inside src_test)
#	--with-dependencies - get jars also from requested package's dependencies
#	  transitively.
#	--virtual - Packages passed to this function are to be handled as virtuals
#	  and will not have individual jar dependencies recorded.
#	--into $dir - symlink jar(s) into $dir (must exist) instead of .
# @param $1 - Package to get jars from, or comma-separated list of packages in
#	case other parameters are not used.
# @param $2 - jar from package. If not specified, all jars will be used.
# @param $3 - When a single jar is specified, destination filename of the
#	symlink. Defaults to the name of the jar.
# ------------------------------------------------------------------------------
# TODO could probably be cleaned up a little
java-pkg_jar-from() {
	debug-print-function ${FUNCNAME} $*

	local build_only=""
	local destdir="."
	local deep=""
	local virtual=""
	local record_jar=""

	[[ "${EBUILD_PHASE}" == "test" ]] && build_only="build"

	while [[ "${1}" == --* ]]; do
		if [[ "${1}" = "--build-only" ]]; then
			build_only="build"
		elif [[ "${1}" = "--with-dependencies" ]]; then
			deep="--with-dependencies"
		elif [[ "${1}" = "--virtual" ]]; then
			virtual="true"
		elif [[ "${1}" = "--into" ]]; then
			destdir="${2}"
			shift
		else
			die "java-pkg_jar-from called with unknown parameter: ${1}"
		fi
		shift
	done

	local target_pkg="${1}" target_jar="${2}" destjar="${3}"

	[[ -z ${target_pkg} ]] && die "Must specify a package"

	if [[ "${EAPI}" == "1" ]]; then
		target_pkg="${target_pkg//:/-}"
	fi

	# default destjar to the target jar
	[[ -z "${destjar}" ]] && destjar="${target_jar}"

	local error_msg="There was a problem getting the classpath for ${target_pkg}."
	local classpath
	classpath="$(java-config ${deep} --classpath=${target_pkg})"
	[[ $? != 0 ]] && die ${error_msg}

	# When we have commas this functions is called to bring jars from multiple
	# packages. This affects recording of dependencencies performed later
	# which expects one package only, so we do it here.
	if [[ ${target_pkg} = *,* ]]; then
		for pkg in ${target_pkg//,/ }; do
			java-pkg_ensure-dep "${build_only}" "${pkg}"
			[[ -z "${build_only}" ]] && java-pkg_record-jar_ "${pkg}"
		done
		# setting this disables further record-jar_ calls later
		record_jar="true"
	else
		java-pkg_ensure-dep "${build_only}" "${target_pkg}"
	fi

	# Record the entire virtual as a dependency so that
	# no jars are missed.
	if [[ -z "${build_only}" && -n "${virtual}" ]]; then
		java-pkg_record-jar_ "${target_pkg}"
		# setting this disables further record-jars_ calls later
		record_jar="true"
	fi

	pushd ${destdir} > /dev/null \
		|| die "failed to change directory to ${destdir}"

	local jar
	for jar in ${classpath//:/ }; do
		local jar_name=$(basename "${jar}")
		if [[ ! -f "${jar}" ]] ; then
			debug-print "${jar} from ${target_pkg} does not exist"
			die "Installation problems with jars in ${target_pkg} - is it installed?"
		fi
		# If no specific target jar was indicated, link it
		if [[ -z "${target_jar}" ]] ; then
			[[ -f "${target_jar}" ]]  && rm "${target_jar}"
			ln -snf "${jar}" \
				|| die "Failed to make symlink from ${jar} to ${jar_name}"
			if [[ -z "${record_jar}" ]]; then
				if [[ -z "${build_only}" ]]; then
					java-pkg_record-jar_ "${target_pkg}" "${jar}"
				else
					java-pkg_record-jar_ --build-only "${target_pkg}" "${jar}"
				fi
			fi
			# otherwise, if the current jar is the target jar, link it
		elif [[ "${jar_name}" == "${target_jar}" ]] ; then
			[[ -f "${destjar}" ]]  && rm "${destjar}"
			ln -snf "${jar}" "${destjar}" \
				|| die "Failed to make symlink from ${jar} to ${destjar}"
			if [[ -z "${record_jar}" ]]; then
				if [[ -z "${build_only}" ]]; then
					java-pkg_record-jar_ "${target_pkg}" "${jar}"
				else
					java-pkg_record-jar_ --build-only "${target_jar}" "${jar}"
				fi
			fi
			popd > /dev/null
			return 0
		fi
	done
	popd > /dev/null
	# if no target was specified, we're ok
	if [[ -z "${target_jar}" ]] ; then
		return 0
	# otherwise, die bitterly
	else
		die "Failed to find ${target_jar:-jar} in ${target_pkg}"
	fi
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_jarfrom
#
# See java-pkg_jar-from
# ------------------------------------------------------------------------------
java-pkg_jarfrom() {
	java-pkg_jar-from "$@"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_getjars
#
# Get the classpath provided by any number of packages
# Among other things, this can be passed to 'javac -classpath' or 'ant -lib'.
# The providing packages are recorded as dependencies into package.env DEPEND
# line, unless "--build-only" is passed as the very first argument, for jars
# that have to be present only at build time and are not needed on runtime
# (junit testing etc).
#
# Example: Get the classpath for xerces-2 and xalan,
#	java-pkg_getjars xerces-2,xalan
# Example Return:
#	/usr/share/xerces-2/lib/xml-apis.jar:/usr/share/xerces-2/lib/xmlParserAPIs.jar:/usr/share/xalan/lib/xalan.jar
#
# @param $opt
#	--build-only - makes the jar(s) not added into package.env DEPEND line.
#	  (assumed automatically when called inside src_test)
#	--with-dependencies - get jars also from requested package's dependencies
#	  transitively.
# @param $1 - list of packages to get jars from
#   (passed to java-config --classpath)
# ------------------------------------------------------------------------------
java-pkg_getjars() {
	debug-print-function ${FUNCNAME} $*

	local build_only=""
	local deep=""

	[[ "${EBUILD_PHASE}" == "test" ]] && build_only="build"

	while [[ "${1}" == --* ]]; do
		if [[ "${1}" = "--build-only" ]]; then
			build_only="build"
		elif [[ "${1}" = "--with-dependencies" ]]; then
			deep="--with-dependencies"
		else
			die "java-pkg_jar-from called with unknown parameter: ${1}"
		fi
		shift
	done

	[[ ${#} -ne 1 ]] && die "${FUNCNAME} takes only one argument besides --*"


	local pkgs="${1}"

	if [[ "${EAPI}" == "1" ]]; then
		pkgs="${pkgs//:/-}"
	fi

	jars="$(java-config ${deep} --classpath=${pkgs})"
	[[ $? != 0 ]] && die "java-config --classpath=${pkgs} failed"
	debug-print "${pkgs}:${jars}"

	for pkg in ${pkgs//,/ }; do
		java-pkg_ensure-dep "${build_only}" "${pkg}"
	done

	for pkg in ${pkgs//,/ }; do
		if [[ -z "${build_only}" ]]; then
			java-pkg_record-jar_ "${pkg}"
		else
			java-pkg_record-jar_ --build-only "${pkg}"
		fi
	done

	echo "${jars}"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_getjar
#
# Get the filename of a single jar from a package
# The providing package is recorded as runtime dependency into package.env
# DEPEND line, unless "--build-only" is passed as the very first argument, for
# jars that have to be present only at build time and are not needed on runtime
# (junit testing etc).
#
# @example
#	java-pkg_getjar xerces-2 xml-apis.jar
# @example-return
#	/usr/share/xerces-2/lib/xml-apis.jar
#
# @param $opt
#	--build-only - makes the jar not added into package.env DEPEND line.
#	--virtual - Packages passed to this function are to be handled as virtuals
#	  and will not have individual jar dependencies recorded.
# @param $1 - package to use
# @param $2 - jar to get
# ------------------------------------------------------------------------------
java-pkg_getjar() {
	debug-print-function ${FUNCNAME} $*

	local build_only=""
	local virtual=""
	local record_jar=""

	[[ "${EBUILD_PHASE}" == "test" ]] && build_only="build"

	while [[ "${1}" == --* ]]; do
		if [[ "${1}" = "--build-only" ]]; then
			build_only="build"
		elif [[ "${1}" == "--virtual" ]]; then
			virtual="true"
		else
			die "java-pkg_getjar called with unknown parameter: ${1}"
		fi
		shift
	done

	[[ ${#} -ne 2 ]] && die "${FUNCNAME} takes only two arguments besides --*"

	local pkg="${1}" target_jar="${2}" jar

	if [[ "${EAPI}" == "1" ]]; then
		pkg="${pkg//:/-}"
	fi

	[[ -z ${pkg} ]] && die "Must specify package to get a jar from"
	[[ -z ${target_jar} ]] && die "Must specify jar to get"

	local error_msg="Could not find classpath for ${pkg}. Are you sure its installed?"
	local classpath
	classpath=$(java-config --classpath=${pkg})
	[[ $? != 0 ]] && die ${error_msg}

	java-pkg_ensure-dep "${build_only}" "${pkg}"

	# Record the package(Virtual) as a dependency and then set build_only
	# So that individual jars are not recorded.
	if [[ -n "${virtual}" ]]; then
		if [[ -z "${build_only}" ]]; then
			java-pkg_record-jar_ "${pkg}"
		else
			java-pkg_record-jar_ --build-only "${pkg}"
		fi
		record_jar="true"
	fi

	for jar in ${classpath//:/ }; do
		if [[ ! -f "${jar}" ]] ; then
			die "Installation problem with jar ${jar} in ${pkg} - is it installed?"
		fi

		if [[ "$(basename ${jar})" == "${target_jar}" ]] ; then
			# Only record jars that aren't build-only
			if [[ -z "${record_jar}" ]]; then
				if [[ -z "${build_only}" ]]; then
					java-pkg_record-jar_ "${pkg}" "${jar}"
				else
					java-pkg_record-jar_ --build-only "${pkg}" "${jar}"
				fi
			fi
			echo "${jar}"
			return 0
		fi
	done

	die "Could not find ${target_jar} in ${pkg}"
	return 1
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_register-dependency
#
# Registers runtime dependency on a package, list of packages, or a single jar
# from a package, into package.env DEPEND line. Can only be called in
# src_install phase.
# Intended for binary packages where you don't need to symlink the jars or get
# their classpath during build. As such, the dependencies only need to be
# specified in ebuild's RDEPEND, and should be omitted in DEPEND.
#
# @param $1 - comma-separated list of packages, or a single package
# @param $2 - if param $1 is a single package, optionally specify the jar
#   to depend on
#
# Example: Record the dependency on whole xerces-2 and xalan,
#	java-pkg_register-dependency xerces-2,xalan
# Example: Record the dependency on ant.jar from ant-core
#	java-pkg_register-dependency ant-core ant.jar
#
# Note: Passing both list of packages as the first parameter AND specifying the
# jar as the second is not allowed and will cause the function to die. We assume
# that there's more chance one passes such combination as a mistake, than that
# there are more packages providing identically named jar without class
# collisions.
# ------------------------------------------------------------------------------
java-pkg_register-dependency() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} -gt 2 ]] && die "${FUNCNAME} takes at most two arguments"

	local pkgs="${1}"
	local jar="${2}"

	[[ -z "${pkgs}" ]] && die "${FUNCNAME} called with no package(s) specified"

	if [[ "${EAPI}" == "1" ]]; then
		pkgs="${pkgs//:/-}"
	fi

	if [[ -z "${jar}" ]]; then
		for pkg in ${pkgs//,/ }; do
			java-pkg_ensure-dep runtime "${pkg}"
			java-pkg_record-jar_ "${pkg}"
		done
	else
		[[ ${pkgs} == *,* ]] && \
			die "${FUNCNAME} called with both package list and jar name"
		java-pkg_ensure-dep runtime "${pkgs}"
		java-pkg_record-jar_ "${pkgs}" "${jar}"
	fi

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_register-optional-dependency
#
# Registers optional runtime dependency on a package, list of packages, or a
# single jar from a package, into package.env OPTIONAL_DEPEND line. Can only be
# called in src_install phase.
# Intended for packages that can use other packages when those are in classpath.
# Will be put on classpath by launcher if they are installed. Typical case is
# JDBC implementations for various databases. It's better than having USE flag
# for each implementation triggering hard dependency.
#
# @param $1 - comma-separated list of packages, or a single package
# @param $2 - if param $1 is a single package, optionally specify the jar
#   to depend on
#
# Example: Record the optional dependency on some jdbc providers
#	java-pkg_register-optional-dependency jdbc-jaybird,jtds-1.2,jdbc-mysql
#
# Note: Passing both list of packages as the first parameter AND specifying the
# jar as the second is not allowed and will cause the function to die. We assume
# that there's more chance one passes such combination as a mistake, than that
# there are more packages providing identically named jar without class
# collisions.
# ------------------------------------------------------------------------------
java-pkg_register-optional-dependency() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} -gt 2 ]] && die "${FUNCNAME} takes at most two arguments"

	local pkgs="${1}"
	local jar="${2}"

	[[ -z "${pkgs}" ]] && die "${FUNCNAME} called with no package(s) specified"

	if [[ "${EAPI}" == "1" ]]; then
		pkgs="${pkgs//:/-}"
	fi

	if [[ -z "${jar}" ]]; then
		for pkg in ${pkgs//,/ }; do
			java-pkg_record-jar_ --optional "${pkg}"
		done
	else
		[[ ${pkgs} == *,* ]] && \
			die "${FUNCNAME} called with both package list and jar name"
		java-pkg_record-jar_ --optional "${pkgs}" "${jar}"
	fi

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_register-environment-variable
#
# Register an arbitrary environment variable into package.env. The gjl launcher
# for this package or any package depending on this will export it into
# environement before executing java command.
# Must only be called in src_install phase.
#
# @param $1 - variable name
# @param $2 - variable value
# ------------------------------------------------------------------------------
JAVA_PKG_EXTRA_ENV="${T}/java-pkg-extra-env"
JAVA_PKG_EXTRA_ENV_VARS=""
java-pkg_register-environment-variable() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_check-phase install

	[[ ${#} != 2 ]] && die "${FUNCNAME} takes two arguments"

	echo "${1}=\"${2}\"" >> ${JAVA_PKG_EXTRA_ENV}
	JAVA_PKG_EXTRA_ENV_VARS="${JAVA_PKG_EXTRA_ENV_VARS} ${1}"

	java-pkg_do_write_
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_get-bootclasspath
#
# Returns classpath of a given bootclasspath-providing package version.
#
# @param $1 - the version of bootclasspath (e.g. 1.5), 'auto' for bootclasspath
#             of the current JDK
# ------------------------------------------------------------------------------

java-pkg_get-bootclasspath() {
	local version="${1}"

	local bcp
	case "${version}" in
		auto)
			bcp="$(java-config -g BOOTCLASSPATH)"
			;;
		1.5)
			bcp="$(java-pkg_getjars --build-only gnu-classpath-0.98)"
			;;
		*)
			eerror "unknown parameter of java-pkg_get-bootclasspath"
			die "unknown parameter of java-pkg_get-bootclasspath"
			;;
	esac

	echo "${bcp}"
}


# This function reads stdin, and based on that input, figures out how to
# populate jars from the filesystem.
# Need to figure out a good way of making use of this, ie be able to use a
# string that was built instead of stdin
# NOTE: this isn't quite ready for primetime.
#java-pkg_populate-jars() {
#	local line
#
#	read line
#	while [[ -n "${line}" ]]; do
#		# Ignore comments
#		[[ ${line%%#*} == "" ]] && continue
#
#		# get rid of any spaces
#		line="${line// /}"
#
#		# format: path=jarinfo
#		local path=${line%%=*}
#		local jarinfo=${line##*=}
#
#		# format: jar@package
#		local jar=${jarinfo%%@*}.jar
#		local package=${jarinfo##*@}
#		if [[ -n ${replace_only} ]]; then
#			[[ ! -f $path ]] && die "No jar exists at ${path}"
#		fi
#		if [[ -n ${create_parent} ]]; then
#			local parent=$(dirname ${path})
#			mkdir -p "${parent}"
#		fi
#		java-pkg_jar-from "${package}" "${jar}" "${path}"
#
#		read line
#	done
#}

# ------------------------------------------------------------------------------
# @section-end query
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @section-begin helper
# @section-summary Helper functions
#
# Various other functions to use from an ebuild
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_find-normal-jars
#
# Find the files with suffix .jar file in the given directory or $WORKDIR
#
# @param $1 - The directory to search for jar files (default: ${WORKDIR})
# ------------------------------------------------------------------------------
java-pkg_find-normal-jars() {
	local dir=$1
	[[ "${dir}" ]] || dir="${WORKDIR}"
	local found
	for jar in $(find "${dir}" -name "*.jar" -type f); do
		echo "${jar}"
		found="true"
	done
	[[ "${found}" ]]
	return $?
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_ensure-no-bundled-jars
#
# Try to locate bundled jar files in ${WORKDIR} and die if found.
# This function should be called after WORKDIR has been populated with symlink
# to system jar files or bundled jars removed.
# ------------------------------------------------------------------------------
java-pkg_ensure-no-bundled-jars() {
	debug-print-function ${FUNCNAME} $*

	local bundled_jars=$(java-pkg_find-normal-jars)
	if [[ -n ${bundled_jars} ]]; then
		echo "Bundled jars found:"
		local jar
		for jar in ${bundled_jars}; do
			echo $(pwd)${jar/./}
		done
		die "Bundled jars found!"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_ensure-vm-version-sufficient
#
# Checks if we have a sufficient VM and dies if we don't.
#
# ------------------------------------------------------------------------------
java-pkg_ensure-vm-version-sufficient() {
	debug-print-function ${FUNCNAME} $*

	if ! java-pkg_is-vm-version-sufficient; then
		debug-print "VM is not suffient"
		eerror "Current Java VM cannot build this package"
		einfo "Please use java-config -S to set the correct one"
		die "Active Java VM cannot build this package"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_is-vm-version-sufficient
#
# @return zero - VM is sufficient
# @return non-zero - VM is not sufficient
# ------------------------------------------------------------------------------
java-pkg_is-vm-version-sufficient() {
	debug-print-function ${FUNCNAME} $*

	depend-java-query --is-sufficient "${DEPEND}" > /dev/null
	return $?
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_ensure-vm-version-eq
#
# Die if the current VM is not equal to the argument passed.
#
# @param $@ - Desired VM version to ensure
# ------------------------------------------------------------------------------
java-pkg_ensure-vm-version-eq() {
	debug-print-function ${FUNCNAME} $*

	if ! java-pkg_is-vm-version-eq $@ ; then
		debug-print "VM is not suffient"
		eerror "This package requires a Java VM version = $@"
		einfo "Please use java-config -S to set the correct one"
		die "Active Java VM too old"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_is-vm-version-eq
#
# @param $@ - VM version to compare current VM to
# @return zero - VM versions are equal
# @return non-zero - VM version are not equal
# ------------------------------------------------------------------------------
java-pkg_is-vm-version-eq() {
	debug-print-function ${FUNCNAME} $*

	local needed_version="$@"

	[[ -z "${needed_version}" ]] && die "need an argument"

	local vm_version="$(java-pkg_get-vm-version)"

	vm_version="$(get_version_component_range 1-2 "${vm_version}")"
	needed_version="$(get_version_component_range 1-2 "${needed_version}")"

	if [[ -z "${vm_version}" ]]; then
		debug-print "Could not get JDK version from DEPEND"
		return 1
	else
		if [[ "${vm_version}" == "${needed_version}" ]]; then
			debug-print "Detected a JDK(${vm_version}) = ${needed_version}"
			return 0
		else
			debug-print "Detected a JDK(${vm_version}) != ${needed_version}"
			return 1
		fi
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_ensure-vm-version-ge
#
# Die if the current VM is not greater than the desired version
#
# @param $@ - VM version to compare current to
# ------------------------------------------------------------------------------
java-pkg_ensure-vm-version-ge() {
	debug-print-function ${FUNCNAME} $*

	if ! java-pkg_is-vm-version-ge "$@" ; then
		debug-print "vm is not suffient"
		eerror "This package requires a Java VM version >= $@"
		einfo "Please use java-config -S to set the correct one"
		die "Active Java VM too old"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_is-vm-version-ge
#
# @param $@ - VM version to compare current VM to
# @return zero - current VM version is greater than checked version
# @return non-zero - current VM version is not greater than checked version
# ------------------------------------------------------------------------------
java-pkg_is-vm-version-ge() {
	debug-print-function ${FUNCNAME} $*

	local needed_version=$@
	local vm_version=$(java-pkg_get-vm-version)
	if [[ -z "${vm_version}" ]]; then
		debug-print "Could not get JDK version from DEPEND"
		return 1
	else
		if version_is_at_least "${needed_version}" "${vm_version}"; then
			debug-print "Detected a JDK(${vm_version}) >= ${needed_version}"
			return 0
		else
			debug-print "Detected a JDK(${vm_version}) < ${needed_version}"
			return 1
		fi
	fi
}

java-pkg_set-current-vm() {
	export GENTOO_VM=${1}
}

java-pkg_get-current-vm() {
	echo ${GENTOO_VM}
}

java-pkg_current-vm-matches() {
	has $(java-pkg_get-current-vm) ${@}
	return $?
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_get-source
#
# Determines what source version should be used, for passing to -source.
# Unless you want to break things you probably shouldn't set _WANT_SOURCE
#
# @return string - Either the lowest possible source, or JAVA_PKG_WANT_SOURCE
# ------------------------------------------------------------------------------
java-pkg_get-source() {
	echo ${JAVA_PKG_WANT_SOURCE:-$(depend-java-query --get-lowest "${DEPEND} ${RDEPEND}")}
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_get-target
#
# Determines what target version should be used, for passing to -target.
# If you don't care about lower versions, you can set _WANT_TARGET to the
# version of your JDK.
#
# @return string - Either the lowest possible target, or JAVA_PKG_WANT_TARGET
# ------------------------------------------------------------------------------
java-pkg_get-target() {
	echo ${JAVA_PKG_WANT_TARGET:-$(depend-java-query --get-lowest "${DEPEND} ${RDEPEND}")}
}

java-pkg_get-javac() {
	debug-print-function ${FUNCNAME} $*


	local compiler="${GENTOO_COMPILER}"

	local compiler_executable
	if [[ "${compiler}" = "javac" ]]; then
		# nothing fancy needs to be done for javac
		compiler_executable="javac"
	else
		# for everything else, try to determine from an env file

		local compiler_env="/usr/share/java-config-2/compiler/${compiler}"
		if [[ -f ${compiler_env} ]]; then
			local old_javac=${JAVAC}
			unset JAVAC
			# try to get value of JAVAC
			compiler_executable="$(source ${compiler_env} 1>/dev/null 2>&1; echo ${JAVAC})"
			export JAVAC=${old_javac}

			if [[ -z ${compiler_executable} ]]; then
				echo "JAVAC is empty or undefined in ${compiler_env}"
				return 1
			fi

			# check that it's executable
			if [[ ! -x ${compiler_executable} ]]; then
				echo "${compiler_executable} doesn't exist, or isn't executable"
				return 1
			fi
		else
			echo "Could not find environment file for ${compiler}"
			return 1
		fi
	fi
	echo ${compiler_executable}
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_javac-args
#
# If an ebuild uses javac directly, instead of using ejavac, it should call this
# to know what -source/-target to use.
#
# @return string - arguments to pass to javac, complete with -target and -source
# ------------------------------------------------------------------------------
java-pkg_javac-args() {
	debug-print-function ${FUNCNAME} $*

	local want_source="$(java-pkg_get-source)"
	local want_target="$(java-pkg_get-target)"

	local source_str="-source ${want_source}"
	local target_str="-target ${want_target}"

	debug-print "want source: ${want_source}"
	debug-print "want target: ${want_target}"

	if [[ -z "${want_source}" || -z "${want_target}" ]]; then
		debug-print "could not find valid -source/-target values for javac"
		echo "Could not find valid -source/-target values for javac"
		return 1
	else
		if java-pkg_is-vm-version-ge "1.4"; then
			echo "${source_str} ${target_str}"
		else
			echo "${target_str}"
		fi
	fi
}

# TODO document
java-pkg_get-jni-cflags() {
	local flags="-I${JAVA_HOME}/include"

	local platform="linux"
	use elibc_FreeBSD && platform="freebsd"

	# TODO do a check that the directories are valid
	flags="${flags} -I${JAVA_HOME}/include/${platform}"

	echo ${flags}
}

java-pkg_ensure-gcj() {
	# was enforcing sys-devel/gcc[gcj]
	die "${FUNCNAME} was removed. Use use-deps available as of EAPI 2 instead. #261562"
}

java-pkg_ensure-test() {
	# was enforcing USE=test if FEATURES=test
	die "${FUNCNAME} was removed. Package mangers handle this already. #278965"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_register-ant-task
#
# Register this package as ant task, so that ant will load it when no specific
# ANT_TASKS are specified. Note that even without this registering, all packages
# specified in ANT_TASKS will be loaded. Mostly used by the actual ant tasks
# packages, but can be also used by other ebuilds that used to symlink their
# .jar into /usr/share/ant-core/lib to get autoloaded, for backwards
# compatibility.
#
# @param --version x.y Register only for ant version x.y (otherwise for any ant
#		version). Used by the ant-* packages to prevent loading of mismatched
#		ant-core ant tasks after core was updated, before the tasks are updated,
#		without a need for blockers.
# @param $1 Name to register as. Defaults to JAVA_PKG_NAME ($PN[-$SLOT])
# ------------------------------------------------------------------------------
java-pkg_register-ant-task() {
	local TASKS_DIR="tasks"

	# check for --version x.y parameters
	while [[ -n "${1}" && -n "${2}" ]]; do
		local var="${1#--}"
		local val="${2}"
		if [[ "${var}" == "version" ]]; then
			TASKS_DIR="tasks-${val}"
		else
			die "Unknown parameter passed to java-pkg_register-ant-tasks: ${1} ${2}"
		fi
		shift 2
	done

	local TASK_NAME="${1:-${JAVA_PKG_NAME}}"

	dodir /usr/share/ant/${TASKS_DIR}
	touch "${D}/usr/share/ant/${TASKS_DIR}/${TASK_NAME}"
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_ant-tasks-depend
#
# Translates the WANT_ANT_TASKS variable into valid dependencies.
# ------------------------------------------------------------------------------
java-pkg_ant-tasks-depend() {
	debug-print-function ${FUNCNAME} ${WANT_ANT_TASKS}

	if [[ -n "${WANT_ANT_TASKS}" ]]; then
		local DEP=""
		for i in ${WANT_ANT_TASKS}
		do
			if [[ ${i} = ant-* ]]; then
				DEP="${DEP}dev-java/${i} "
			elif [[ ${i} = */*:* ]]; then
				DEP="${DEP}${i} "
			else
				echo "Invalid atom in WANT_ANT_TASKS: ${i}"
				return 1
			fi
		done
		echo ${DEP}
		return 0
	else
		return 0
	fi
}


# ------------------------------------------------------------------------------
# @internal-function ejunit_
#
# Internal Junit wrapper function. Makes it easier to run the tests and checks for
# dev-java/junit in DEPEND. Launches the tests using junit.textui.TestRunner.
#
# @param $1 - junit package (junit or junit-4)
# @param $2 - -cp or -classpath
# @param $3 - classpath; junit and recorded dependencies get appended
# @param $@ - the rest of the parameters are passed to java
ejunit_() {
	debug-print-function ${FUNCNAME} $*

	local pkgs
	if [[ -f ${JAVA_PKG_DEPEND_FILE} ]]; then
		for atom in $(cat ${JAVA_PKG_DEPEND_FILE} | tr : ' '); do
			pkgs=${pkgs},$(echo ${atom} | sed -re "s/^.*@//")
		done
	fi

	local junit=${1}
	shift 1

	local cp=$(java-pkg_getjars --with-dependencies ${junit}${pkgs})
	if [[ ${1} = -cp || ${1} = -classpath ]]; then
		cp="${2}:${cp}"
		shift 2
	else
		cp=".:${cp}"
	fi

	local runner=junit.textui.TestRunner
	if [[ "${junit}" == "junit-4" ]] ; then
		runner=org.junit.runner.JUnitCore
	fi
	debug-print "Calling: java -cp \"${cp}\" -Djava.awt.headless=true ${runner} ${@}"
	java -cp "${cp}" -Djava.awt.headless=true ${runner} "${@}" || die "Running junit failed"
}

# ------------------------------------------------------------------------------
# @ebuild-function ejunit
#
# Junit wrapper function. Makes it easier to run the tests and checks for
# dev-java/junit in DEPEND. Launches the tests using org.junit.runner.JUnitCore.
#
# Examples:
# ejunit -cp build/classes org.blinkenlights.jid3.test.AllTests
# ejunit org.blinkenlights.jid3.test.AllTests
# ejunit org.blinkenlights.jid3.test.FirstTest \
#         org.blinkenlights.jid3.test.SecondTest
#
# @param $1 - -cp or -classpath
# @param $2 - classpath; junit and recorded dependencies get appended
# @param $@ - the rest of the parameters are passed to java
# ------------------------------------------------------------------------------
ejunit() {
	debug-print-function ${FUNCNAME} $*

	ejunit_ "junit" "${@}"
}

# ------------------------------------------------------------------------------
# @ebuild-function ejunit4
#
# Junit4 wrapper function. Makes it easier to run the tests and checks for
# dev-java/junit:4 in DEPEND. Launches the tests using junit.textui.TestRunner.
#
# Examples:
# ejunit4 -cp build/classes org.blinkenlights.jid3.test.AllTests
# ejunit4 org.blinkenlights.jid3.test.AllTests
# ejunit4 org.blinkenlights.jid3.test.FirstTest \
#         org.blinkenlights.jid3.test.SecondTest
#
# @param $1 - -cp or -classpath
# @param $2 - classpath; junit and recorded dependencies get appended
# @param $@ - the rest of the parameters are passed to java
# ------------------------------------------------------------------------------
ejunit4() {
	debug-print-function ${FUNCNAME} $*

	ejunit_ "junit-4" "${@}"
}

# ------------------------------------------------------------------------------
# @section-end helper
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @eclass-src_prepare
#
# src_prepare Searches for bundled jars
# Don't call directly, but via java-pkg-2_src_prepare!
# ------------------------------------------------------------------------------

java-utils-2_src_prepare() {
	[[ ${EBUILD_PHASE} == prepare ]] &&
		java-pkg_func-exists java_prepare && java_prepare

	# Remember that eant will call this unless called via Portage
	if [[ ! -e "${T}/java-utils-2_src_prepare-run" ]] && is-java-strict; then
		echo "Searching for bundled jars:"
		java-pkg_find-normal-jars || echo "None found."
		echo "Searching for bundled classes (no output if none found):"
		find "${WORKDIR}" -name "*.class"
		echo "Search done."
	fi
	touch "${T}/java-utils-2_src_prepare-run"
}

# ------------------------------------------------------------------------------
# @eclass-pkg_preinst
#
# pkg_preinst Searches for missing and unneeded dependencies
# Don't call directly, but via java-pkg-2_pkg_preinst!
# ------------------------------------------------------------------------------

java-utils-2_pkg_preinst() {
	if is-java-strict; then
		if has_version dev-java/java-dep-check; then
			[[ -e "${JAVA_PKG_ENV}" ]] || return
			local output=$(GENTOO_VM= java-dep-check --image "${D}" "${JAVA_PKG_ENV}")
			if [[ ${output} && has_version <=dev-java/java-dep-check-0.2 ]]; then
				ewarn "Possibly unneeded dependencies found in package.env:"
				for dep in ${output}; do
					ewarn "\t${dep}"
				done
			fi
			if [[ ${output} && has_version >dev-java/java-dep-check-0.2 ]]; then
				ewarn "${output}"
			fi
		else
			eerror "Install dev-java/java-dep-check for dependency checking"
		fi
	fi
}

# ------------------------------------------------------------------------------
# @section-begin build
# @section-summary Build functions
#
# These are some functions for building a package. In particular, it consists of
# wrappers for javac and ant.
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @ebuild-function eant
#
# Ant wrapper function. Will use the appropriate compiler, based on user-defined
# compiler. Will also set proper ANT_TASKS from the variable ANT_TASKS,
# variables:
# EANT_GENTOO_CLASSPATH - calls java-pkg_getjars for the value and adds to the
#                         gentoo.classpath property. Be sure to call
#                         java-ant_rewrite-classpath in src_unpack.
# EANT_NEEDS_TOOLS - add tools.jar to the gentoo.classpath. Should only be used
#                    for build-time purposes, the dependency is not recorded to
#                    package.env!
# *ANT_TASKS - used to determine ANT_TASKS before calling Ant.
# ------------------------------------------------------------------------------
eant() {
	debug-print-function ${FUNCNAME} $*

	if [[ ${EBUILD_PHASE} = compile ]]; then
		java-ant-2_src_configure
		java-utils-2_src_prepare
	fi

	if ! has java-ant-2 ${INHERITED}; then
		local msg="You should inherit java-ant-2 when using eant"
		java-pkg_announce-qa-violation "${msg}"
	fi

	local antflags="-Dnoget=true -Dmaven.mode.offline=true -Dbuild.sysclasspath=ignore"

	java-pkg_init-compiler_
	local compiler="${GENTOO_COMPILER}"

	local compiler_env="${JAVA_PKG_COMPILER_DIR}/${compiler}"
	local build_compiler="$(source ${compiler_env} 1>/dev/null 2>&1; echo ${ANT_BUILD_COMPILER})"
	if [[ "${compiler}" != "javac" && -z "${build_compiler}" ]]; then
		die "ANT_BUILD_COMPILER undefined in ${compiler_env}"
	fi

	if [[ ${compiler} != "javac" ]]; then
		antflags="${antflags} -Dbuild.compiler=${build_compiler}"
		# Figure out any extra stuff to put on the classpath for compilers aside
		# from javac
		# ANT_BUILD_COMPILER_DEPS should be something that could be passed to
		# java-config -p
		local build_compiler_deps="$(source ${JAVA_PKG_COMPILER_DIR}/${compiler} 1>/dev/null 2>&1; echo ${ANT_BUILD_COMPILER_DEPS})"
		if [[ -n ${build_compiler_deps} ]]; then
			antflags="${antflags} -lib $(java-config -p ${build_compiler_deps})"
		fi
	fi

	for arg in "${@}"; do
		if [[ ${arg} = -lib ]]; then
			if is-java-strict; then
				eerror "You should not use the -lib argument to eant because it will fail"
				eerror "with JAVA_PKG_STRICT. Please use for example java-pkg_jar-from"
				eerror "or ant properties to make dependencies available."
				eerror "For ant tasks use WANT_ANT_TASKS or ANT_TASKS from."
				eerror "split ant (>=dev-java/ant-core-1.7)."
				die "eant -lib is deprecated/forbidden"
			else
				echo "eant -lib is deprecated. Turn JAVA_PKG_STRICT on for"
				echo "more info."
			fi
		fi
	done

	# parse WANT_ANT_TASKS for atoms
	local want_ant_tasks
	for i in ${WANT_ANT_TASKS}; do
		if [[ ${i} = */*:* ]]; then
			i=${i#*/}
			i=${i%:0}
			want_ant_tasks+="${i/:/-} "
		else
			want_ant_tasks+="${i} "
		fi
	done
	# default ANT_TASKS to WANT_ANT_TASKS, if ANT_TASKS is not set explicitly
	ANT_TASKS="${ANT_TASKS:-${want_ant_tasks% }}"

	# override ANT_TASKS with JAVA_PKG_FORCE_ANT_TASKS if it's set
	ANT_TASKS="${JAVA_PKG_FORCE_ANT_TASKS:-${ANT_TASKS}}"

	# if ant-tasks is not set by ebuild or forced, use none
	ANT_TASKS="${ANT_TASKS:-none}"

	# at this point, ANT_TASKS should be "all", "none" or explicit list
	if [[ "${ANT_TASKS}" == "all" ]]; then
		einfo "Using all available ANT_TASKS"
	elif [[ "${ANT_TASKS}" == "none" ]]; then
		einfo "Disabling all optional ANT_TASKS"
	else
		einfo "Using following ANT_TASKS: ${ANT_TASKS}"
	fi

	export ANT_TASKS

	[[ -n ${JAVA_PKG_DEBUG} ]] && antflags="${antflags} --execdebug -debug"
	[[ -n ${PORTAGE_QUIET} ]] && antflags="${antflags} -q"

	local gcp="${EANT_GENTOO_CLASSPATH}"
	local getjarsarg=""

	if [[ ${EBUILD_PHASE} = "test" ]]; then
		antflags="${antflags} -DJunit.present=true"
		[[ ${ANT_TASKS} = *ant-junit* ]] && gcp="${gcp} junit"
		getjarsarg="--with-dependencies"
	else
		antflags="${antflags} -Dmaven.test.skip=true"
	fi

	local cp

	for atom in ${gcp}; do
		cp="${cp}:$(java-pkg_getjars ${getjarsarg} ${atom})"
	done

	[[ -n "${EANT_NEEDS_TOOLS}" ]] && cp="${cp}:$(java-config --tools)"

	if [[ ${cp} ]]; then
		# It seems ant does not like single quotes around ${cp}
		cp=${cp#:}
		[[ ${EANT_GENTOO_CLASSPATH_EXTRA} ]] && \
			cp="${cp}:${EANT_GENTOO_CLASSPATH_EXTRA}"
		antflags="${antflags} -Dgentoo.classpath=\"${cp}\""
	fi

	[[ -n ${JAVA_PKG_DEBUG} ]] && echo ant ${antflags} "${@}"
	debug-print "Calling ant (GENTOO_VM: ${GENTOO_VM}): ${antflags} ${@}"
	ant ${antflags} "${@}" || die "eant failed"
}

# ------------------------------------------------------------------------------
# @ebuild-function ejavac
#
# Javac wrapper function. Will use the appropriate compiler, based on
# /etc/java-config/compilers.conf
#
# @param $@ - Arguments to be passed to the compiler
# ------------------------------------------------------------------------------
ejavac() {
	debug-print-function ${FUNCNAME} $*

	java-pkg_init-compiler_

	local compiler_executable
	compiler_executable=$(java-pkg_get-javac)
	if [[ ${?} != 0 ]]; then
		eerror "There was a problem determining compiler: ${compiler_executable}"
		die "get-javac failed"
	fi

	local javac_args
	javac_args="$(java-pkg_javac-args)"
	if [[ ${?} != 0 ]]; then
		eerror "There was a problem determining JAVACFLAGS: ${javac_args}"
		die "java-pkg_javac-args failed"
	fi

	[[ -n ${JAVA_PKG_DEBUG} ]] && echo ${compiler_executable} ${javac_args} "${@}"
	${compiler_executable} ${javac_args} "${@}" || die "ejavac failed"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_filter-compiler
#
# Used to prevent the use of some compilers. Should be used in src_compile.
# Basically, it just appends onto JAVA_PKG_FILTER_COMPILER
#
# @param $@ - compilers to filter
# ------------------------------------------------------------------------------
java-pkg_filter-compiler() {
	JAVA_PKG_FILTER_COMPILER="${JAVA_PKG_FILTER_COMPILER} $@"
}

# ------------------------------------------------------------------------------
# @ebuild-function java-pkg_force-compiler
#
# Used to force the use of particular compilers. Should be used in src_compile.
# A common use of this would be to force ecj-3.1 to be used on amd64, to avoid
# OutOfMemoryErrors that may come up.
#
# @param $@ - compilers to force
# ------------------------------------------------------------------------------
java-pkg_force-compiler() {
	JAVA_PKG_FORCE_COMPILER="$@"
}

# ------------------------------------------------------------------------------
# @ebuild-function use_doc
#
# Helper function for getting ant to build javadocs. If the user has USE=doc,
# then 'javadoc' or the argument are returned. Otherwise, there is no return.
#
# The output of this should be passed to ant.
#
# Example: build javadocs by calling 'javadoc' target
#	eant $(use_doc)
# Example: build javadocs by calling 'apidoc' target
#	eant $(use_doc apidoc)
#
# @param $@ - Option value to return. Defaults to 'javadoc'
# @return string - Name of the target to create javadocs
# ------------------------------------------------------------------------------
use_doc() {
	use doc && echo ${@:-javadoc}
}


# ------------------------------------------------------------------------------
# @section-end build
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# @section-begin internal
# @section-summary Internal functions
#
# Do __NOT__ use any of these from an ebuild! These are only to be used from
# within the java eclasses.
# ------------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# @function-internal java-pkg_init
#
# The purpose of this function, as the name might imply, is to initialize the
# Java environment. It ensures that that there aren't any environment variables
# that'll muss things up. It initializes some variables, which are used
# internally. And most importantly, it'll switch the VM if necessary.
#
# This shouldn't be used directly. Instead, java-pkg and java-pkg-opt will
# call it during each of the phases of the merge process.
#
# -----------------------------------------------------------------------------
java-pkg_init() {
	debug-print-function ${FUNCNAME} $*

	# Don't set up build environment if installing from binary. #206024 #258423
	[[ "${MERGE_TYPE}" == "binary" ]] && return
	# Also try Portage's nonstandard EMERGE_FROM for old EAPIs, if it doesn't
	# work nothing is lost.
	has ${EAPI:-0} 0 1 2 3 && [[ "${EMERGE_FROM}" == "binary" ]] && return

	unset JAVAC
	unset JAVA_HOME

	java-config --help >/dev/null || {
		eerror ""
		eerror "Can't run java-config --help"
		eerror "Have you upgraded python recently but haven't"
		eerror "run python-updater yet?"
		die "Can't run java-config --help"
	}

	# People do all kinds of weird things.
	# http://forums.gentoo.org/viewtopic-p-3943166.html
	local silence="${SILENCE_JAVA_OPTIONS_WARNING}"
	local accept="${I_WANT_GLOBAL_JAVA_OPTIONS}"
	if [[ -n ${_JAVA_OPTIONS} && -z ${accept} && -z ${silence} ]]; then
		ewarn "_JAVA_OPTIONS changes what java -version outputs at least for"
		ewarn "sun-jdk vms and and as such break configure scripts that"
		ewarn "use it (for example app-office/openoffice) so we filter it out."
		ewarn "Use SILENCE_JAVA_OPTIONS_WARNING=true in the environment (use"
		ewarn "make.conf for example) to silence this warning or"
		ewarn "I_WANT_GLOBAL_JAVA_OPTIONS to not filter it."
	fi

	if [[ -z ${accept} ]]; then
		# export _JAVA_OPTIONS= doesn't work because it will show up in java
		# -version output
		unset _JAVA_OPTIONS
		# phase hooks make this run many times without this
		I_WANT_GLOBAL_JAVA_OPTIONS="true"
	fi

	if java-pkg_func-exists ant_src_unpack; then
		java-pkg_announce-qa-violation "Using old ant_src_unpack. Should be src_unpack"
	fi

	java-pkg_init_paths_
	java-pkg_switch-vm
	PATH=${JAVA_HOME}/bin:${PATH}

	# TODO we will probably want to set JAVAC and JAVACFLAGS

	# Do some QA checks
	java-pkg_check-jikes

	# Can't use unset here because Portage does not save the unset
	# see https://bugs.gentoo.org/show_bug.cgi?id=189417#c11

	# When users have crazy classpaths some packages can fail to compile.
	# and everything should work with empty CLASSPATH.
	# This also helps prevent unexpected dependencies on random things
	# from the CLASSPATH.
	export CLASSPATH=

	# Unset external ANT_ stuff
	export ANT_TASKS=
	export ANT_OPTS=
	export ANT_RESPECT_JAVA_HOME=
}

# ------------------------------------------------------------------------------
# @function-internal java-pkg-init-compiler_
#
# This function attempts to figure out what compiler should be used. It does
# this by reading the file at JAVA_PKG_COMPILERS_CONF, and checking the
# COMPILERS variable defined there.
# This can be overridden by a list in JAVA_PKG_FORCE_COMPILER
#
# It will go through the list of compilers, and verify that it supports the
# target and source that are needed. If it is not suitable, then the next
# compiler is checked. When JAVA_PKG_FORCE_COMPILER is defined, this checking
# isn't done.
#
# Once the which compiler to use has been figured out, it is set to
# GENTOO_COMPILER.
#
# If you hadn't guessed, JAVA_PKG_FORCE_COMPILER is for testing only.
#
# If the user doesn't defined anything in JAVA_PKG_COMPILERS_CONF, or no
# suitable compiler was found there, then the default is to use javac provided
# by the current VM.
#
#
# @return name of the compiler to use
# ------------------------------------------------------------------------------
java-pkg_init-compiler_() {
	debug-print-function ${FUNCNAME} $*

	if [[ -n ${GENTOO_COMPILER} ]]; then
		debug-print "GENTOO_COMPILER already set"
		return
	fi

	local compilers;
	if [[ -z ${JAVA_PKG_FORCE_COMPILER} ]]; then
		compilers="$(source ${JAVA_PKG_COMPILERS_CONF} 1>/dev/null 2>&1; echo	${COMPILERS})"
	else
		compilers=${JAVA_PKG_FORCE_COMPILER}
	fi

	debug-print "Read \"${compilers}\" from ${JAVA_PKG_COMPILERS_CONF}"

	# Figure out if we should announce what compiler we're using
	local compiler
	for compiler in ${compilers}; do
		debug-print "Checking ${compiler}..."
		# javac should always be alright
		if [[ ${compiler} = "javac" ]]; then
			debug-print "Found javac... breaking"
			export GENTOO_COMPILER="javac"
			break
		fi

		if has ${compiler} ${JAVA_PKG_FILTER_COMPILER}; then
			if [[ -z ${JAVA_PKG_FORCE_COMPILER} ]]; then
				einfo "Filtering ${compiler}"
				continue
			fi
		fi

		# for non-javac, we need to make sure it supports the right target and
		# source
		local compiler_env="${JAVA_PKG_COMPILER_DIR}/${compiler}"
		if [[ -f ${compiler_env} ]]; then
			local desired_target="$(java-pkg_get-target)"
			local desired_source="$(java-pkg_get-source)"


			# Verify that the compiler supports target
			local supported_target=$(source ${compiler_env} 1>/dev/null 2>&1; echo ${SUPPORTED_TARGET})
			if ! has ${desired_target} ${supported_target}; then
				ewarn "${compiler} does not support -target ${desired_target},	skipping"
				continue
			fi

			# -source was introduced in 1.3, so only check 1.3 and on
			if version_is_at_least "${desired_soure}" "1.3"; then
				# Verify that the compiler supports source
				local supported_source=$(source ${compiler_env} 1>/dev/null 2>&1; echo ${SUPPORTED_SOURCE})
				if ! has ${desired_source} ${supported_source}; then
					ewarn "${compiler} does not support -source ${desired_source}, skipping"
					continue
				fi
			fi

			# if you get here, then the compiler should be good to go
			export GENTOO_COMPILER="${compiler}"
			break
		else
			ewarn "Could not find configuration for ${compiler}, skipping"
			ewarn "Perhaps it is not installed?"
			continue
		fi
	done

	# If it hasn't been defined already, default to javac
	if [[ -z ${GENTOO_COMPILER} ]]; then
		if [[ -n ${compilers} ]]; then
			einfo "No suitable compiler found: defaulting to JDK default for compilation"
		else
			# probably don't need to notify users about the default.
			:;#einfo "Defaulting to javac for compilation"
		fi
		if java-config -g GENTOO_COMPILER 2> /dev/null; then
			export GENTOO_COMPILER=$(java-config -g GENTOO_COMPILER)
		else
			export GENTOO_COMPILER=javac
		fi
	else
		einfo "Using ${GENTOO_COMPILER} for compilation"
	fi

}

# ------------------------------------------------------------------------------
# @internal-function init_paths_
#
# Initializes some variables that will be used. These variables are mostly used
# to determine where things will eventually get installed.
# ------------------------------------------------------------------------------
java-pkg_init_paths_() {
	debug-print-function ${FUNCNAME} $*

	local pkg_name
	if [[ "$SLOT" == "0" ]] ; then
		JAVA_PKG_NAME="${PN}"
	else
		JAVA_PKG_NAME="${PN}-${SLOT}"
	fi

	JAVA_PKG_SHAREPATH="${DESTTREE}/share/${JAVA_PKG_NAME}"
	JAVA_PKG_SOURCESPATH="${JAVA_PKG_SHAREPATH}/sources/"
	JAVA_PKG_ENV="${D}${JAVA_PKG_SHAREPATH}/package.env"
	JAVA_PKG_VIRTUALS_PATH="${DESTTREE}/share/java-config-2/virtuals"
	JAVA_PKG_VIRTUAL_PROVIDER="${D}/${JAVA_PKG_VIRTUALS_PATH}/${JAVA_PKG_NAME}"

	[[ -z "${JAVA_PKG_JARDEST}" ]] && JAVA_PKG_JARDEST="${JAVA_PKG_SHAREPATH}/lib"
	[[ -z "${JAVA_PKG_LIBDEST}" ]] && JAVA_PKG_LIBDEST="${DESTTREE}/$(get_libdir)/${JAVA_PKG_NAME}"
	[[ -z "${JAVA_PKG_WARDEST}" ]] && JAVA_PKG_WARDEST="${JAVA_PKG_SHAREPATH}/webapps"


	# TODO maybe only print once?
	debug-print "JAVA_PKG_SHAREPATH: ${JAVA_PKG_SHAREPATH}"
	debug-print "JAVA_PKG_ENV: ${JAVA_PKG_ENV}"
	debug-print "JAVA_PKG_JARDEST: ${JAVA_PKG_JARDEST}"
	debug-print "JAVA_PKG_LIBDEST: ${JAVA_PKG_LIBDEST}"
	debug-print "JAVA_PKG_WARDEST: ${JAVA_PKG_WARDEST}"
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_do_write_
#
# Writes the package.env out to disk.
#
# ------------------------------------------------------------------------------
# TODO change to do-write, to match everything else
java-pkg_do_write_() {
	debug-print-function ${FUNCNAME} $*
	java-pkg_init_paths_
	# Create directory for package.env
	dodir "${JAVA_PKG_SHAREPATH}"
	if [[ -n "${JAVA_PKG_CLASSPATH}" || -n "${JAVA_PKG_LIBRARY}" || -f \
			"${JAVA_PKG_DEPEND_FILE}" || -f \
			"${JAVA_PKG_OPTIONAL_DEPEND_FILE}" ]]; then
		# Create package.env
		(
			echo "DESCRIPTION=\"${DESCRIPTION}\""
			echo "GENERATION=\"2\""
			echo "SLOT=\"${SLOT}\""
			echo "CATEGORY=\"${CATEGORY}\""
			echo "PVR=\"${PVR}\""

			[[ -n "${JAVA_PKG_CLASSPATH}" ]] && echo "CLASSPATH=\"${JAVA_PKG_CLASSPATH}\""
			[[ -n "${JAVA_PKG_LIBRARY}" ]] && echo "LIBRARY_PATH=\"${JAVA_PKG_LIBRARY}\""
			[[ -n "${JAVA_PROVIDE}" ]] && echo "PROVIDES=\"${JAVA_PROVIDE}\""
			[[ -f "${JAVA_PKG_DEPEND_FILE}" ]] \
				&& echo "DEPEND=\"$(sort -u "${JAVA_PKG_DEPEND_FILE}" | tr '\n' ':')\""
			[[ -f "${JAVA_PKG_OPTIONAL_DEPEND_FILE}" ]] \
				&& echo "OPTIONAL_DEPEND=\"$(sort -u "${JAVA_PKG_OPTIONAL_DEPEND_FILE}" | tr '\n' ':')\""
			echo "VM=\"$(echo ${RDEPEND} ${DEPEND} | sed -e 's/ /\n/g' | sed -n -e '/virtual\/\(jre\|jdk\)/ { p;q }')\"" # TODO cleanup !
			[[ -f "${JAVA_PKG_BUILD_DEPEND_FILE}" ]] \
				&& echo "BUILD_DEPEND=\"$(sort -u "${JAVA_PKG_BUILD_DEPEND_FILE}" | tr '\n' ':')\""
		) > "${JAVA_PKG_ENV}"

		# register target/source
		local target="$(java-pkg_get-target)"
		local source="$(java-pkg_get-source)"
		[[ -n ${target} ]] && echo "TARGET=\"${target}\"" >> "${JAVA_PKG_ENV}"
		[[ -n ${source} ]] && echo "SOURCE=\"${source}\"" >> "${JAVA_PKG_ENV}"

		# register javadoc info
		[[ -n ${JAVADOC_PATH} ]] && echo "JAVADOC_PATH=\"${JAVADOC_PATH}\"" \
			>> ${JAVA_PKG_ENV}
		# register source archives
		[[ -n ${JAVA_SOURCES} ]] && echo "JAVA_SOURCES=\"${JAVA_SOURCES}\"" \
			>> ${JAVA_PKG_ENV}


		echo "MERGE_VM=\"${GENTOO_VM}\"" >> "${JAVA_PKG_ENV}"
		[[ -n ${GENTOO_COMPILER} ]] && echo "MERGE_COMPILER=\"${GENTOO_COMPILER}\"" >> "${JAVA_PKG_ENV}"

		# extra env variables
		if [[ -n "${JAVA_PKG_EXTRA_ENV_VARS}" ]]; then
			cat "${JAVA_PKG_EXTRA_ENV}" >> "${JAVA_PKG_ENV}" || die
			# nested echo to remove leading/trailing spaces
			echo "ENV_VARS=\"$(echo ${JAVA_PKG_EXTRA_ENV_VARS})\"" \
				>> "${JAVA_PKG_ENV}" || die
		fi

		# Strip unnecessary leading and trailing colons
		# TODO try to cleanup if possible
		sed -e "s/=\":/=\"/" -e "s/:\"$/\"/" -i "${JAVA_PKG_ENV}" || die "Did you forget to call java_init ?"
	else
		debug-print "JAVA_PKG_CLASSPATH, JAVA_PKG_LIBRARY, JAVA_PKG_DEPEND_FILE"
		debug-print "or JAVA_PKG_OPTIONAL_DEPEND_FILE not defined so can't"
		debug-print "write package.env."
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_record-jar_
#
# Record an (optional) dependency to the package.env
# @param --optional - record dependency as optional
# @param --build - record dependency as build_only
# @param $1 - package to record
# @param $2 - (optional) jar of package to record
# ------------------------------------------------------------------------------
JAVA_PKG_DEPEND_FILE="${T}/java-pkg-depend"
JAVA_PKG_OPTIONAL_DEPEND_FILE="${T}/java-pkg-optional-depend"
JAVA_PKG_BUILD_DEPEND_FILE="${T}/java-pkg-build-depend"

java-pkg_record-jar_() {
	debug-print-function ${FUNCNAME} $*

	local depend_file="${JAVA_PKG_DEPEND_FILE}"
	case "${1}" in
		"--optional") depend_file="${JAVA_PKG_OPTIONAL_DEPEND_FILE}"; shift;;
		"--build-only") depend_file="${JAVA_PKG_BUILD_DEPEND_FILE}"; shift;;
	esac

	local pkg=${1} jar=${2} append
	if [[ -z "${jar}" ]]; then
		append="${pkg}"
	else
		append="$(basename ${jar})@${pkg}"
	fi

	echo "${append}" >> "${depend_file}"
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_append_
#
# Appends a value to a variable
#
# Example: java-pkg_append_ CLASSPATH foo.jar
# @param $1 variable name to modify
# @param $2 value to append
# ------------------------------------------------------------------------------
java-pkg_append_() {
	debug-print-function ${FUNCNAME} $*

	local var="${1}" value="${2}"
	if [[ -z "${!var}" ]] ; then
		export ${var}="${value}"
	else
		local oldIFS=${IFS} cur haveit
		IFS=':'
		for cur in ${!var}; do
			if [[ ${cur} == ${value} ]]; then
				haveit="yes"
				break
			fi
		done
		[[ -z ${haveit} ]] && export ${var}="${!var}:${value}"
		IFS=${oldIFS}
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_expand_dir_
#
# Gets the full path of the file/directory's parent.
# @param $1 - file/directory to find parent directory for
# @return - path to $1's parent directory
# ------------------------------------------------------------------------------
java-pkg_expand_dir_() {
	pushd "$(dirname "${1}")" >/dev/null 2>&1
	pwd
	popd >/dev/null 2>&1
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_func-exists
#
# Does the indicated function exist?
#
# @return 0 - function is declared
# @return 1 - function is undeclared
# ------------------------------------------------------------------------------
java-pkg_func-exists() {
	declare -F ${1} > /dev/null
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_setup-vm
#
# Sets up the environment for a specific VM
#
# ------------------------------------------------------------------------------
java-pkg_setup-vm() {
	debug-print-function ${FUNCNAME} $*

	local vendor="$(java-pkg_get-vm-vendor)"
	if [[ "${vendor}" == "sun" ]] && java-pkg_is-vm-version-ge "1.5" ; then
		addpredict "/dev/random"
	elif [[ "${vendor}" == "ibm" ]]; then
		addpredict "/proc/self/maps"
		addpredict "/proc/cpuinfo"
		addpredict "/proc/self/coredump_filter"
	elif [[ "${vendor}" == "oracle" ]]; then
		addpredict "/dev/random"
		addpredict "/proc/self/coredump_filter"
	elif [[ "${vendor}" == icedtea* ]] && java-pkg_is-vm-version-ge "1.7" ; then
		addpredict "/dev/random"
		addpredict "/proc/self/coredump_filter"
	elif [[ "${vendor}" == "jrockit" ]]; then
		addpredict "/proc/cpuinfo"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_needs-vm
#
# Does the current package depend on virtual/jdk or does it set
# JAVA_PKG_WANT_BUILD_VM?
#
# @return 0 - Package depends on virtual/jdk
# @return 1 - Package does not depend on virtual/jdk
# ------------------------------------------------------------------------------
java-pkg_needs-vm() {
	debug-print-function ${FUNCNAME} $*

	if [[ -n "$(echo ${JAVA_PKG_NV_DEPEND:-${DEPEND}} | sed -e '\:virtual/jdk:!d')" ]]; then
		return 0
	fi

	[[ -n "${JAVA_PKG_WANT_BUILD_VM}" ]] && return 0

	return 1
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_get-current-vm
#
# @return - The current VM being used
# ------------------------------------------------------------------------------
java-pkg_get-current-vm() {
	java-config -f
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_get-vm-vendor
#
# @return - The vendor of the current VM
# ------------------------------------------------------------------------------
java-pkg_get-vm-vendor() {
	debug-print-function ${FUNCNAME} $*

	local vm="$(java-pkg_get-current-vm)"
	vm="${vm/-*/}"
	echo "${vm}"
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_get-vm-version
#
# @return - The version of the current VM
# ------------------------------------------------------------------------------
java-pkg_get-vm-version() {
	debug-print-function ${FUNCNAME} $*

	java-config -g PROVIDES_VERSION
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_build-vm-from-handle
#
# Selects a build vm from a list of vm handles. First checks for the system-vm
# beeing usable, then steps through the listed handles till a suitable vm is
# found.
#
# @return - VM handle of an available JDK
# ------------------------------------------------------------------------------
java-pkg_build-vm-from-handle() {
	debug-print-function ${FUNCNAME} "$*"

	local vm
	vm=$(java-pkg_get-current-vm 2>/dev/null)
	if [[ $? -eq 0 ]]; then
		if has ${vm} ${JAVA_PKG_WANT_BUILD_VM}; then
			echo ${vm}
			return 0
		fi
	fi

	for vm in ${JAVA_PKG_WANT_BUILD_VM}; do
		if java-config-2 --select-vm=${vm} 2>/dev/null; then
			echo ${vm}
			return 0
		fi
	done

	eerror "${FUNCNAME}: No vm found for handles: ${JAVA_PKG_WANT_BUILD_VM}"
	return 1
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_switch-vm
#
# Switch VM if we're allowed to (controlled by JAVA_PKG_ALLOW_VM_CHANGE), and
# verify that the current VM is sufficient.
# Setup the environment for the VM being used.
# ------------------------------------------------------------------------------
java-pkg_switch-vm() {
	debug-print-function ${FUNCNAME} $*

	if java-pkg_needs-vm; then
		# Use the VM specified by JAVA_PKG_FORCE_VM
		if [[ -n "${JAVA_PKG_FORCE_VM}" ]]; then
			# If you're forcing the VM, I hope you know what your doing...
			debug-print "JAVA_PKG_FORCE_VM used: ${JAVA_PKG_FORCE_VM}"
			export GENTOO_VM="${JAVA_PKG_FORCE_VM}"
		# if we're allowed to switch the vm...
		elif [[ "${JAVA_PKG_ALLOW_VM_CHANGE}" == "yes" ]]; then
			# if there is an explicit list of handles to choose from
			if [[ -n "${JAVA_PKG_WANT_BUILD_VM}" ]]; then
				debug-print "JAVA_PKG_WANT_BUILD_VM used: ${JAVA_PKG_WANT_BUILD_VM}"
				GENTOO_VM=$(java-pkg_build-vm-from-handle)
				if [[ $? != 0 ]]; then
					eerror "${FUNCNAME}: No VM found for handles: ${JAVA_PKG_WANT_BUILD_VM}"
					die "${FUNCNAME}: Failed to determine VM for building"
				fi
				# JAVA_PKG_WANT_SOURCE and JAVA_PKG_WANT_TARGET are required as
				# they can't be deduced from handles.
				if [[ -z "${JAVA_PKG_WANT_SOURCE}" ]]; then
					eerror "JAVA_PKG_WANT_BUILD_VM specified but not JAVA_PKG_WANT_SOURCE"
					die "Specify JAVA_PKG_WANT_SOURCE"
				fi
				if [[ -z "${JAVA_PKG_WANT_TARGET}" ]]; then
					eerror "JAVA_PKG_WANT_BUILD_VM specified but not JAVA_PKG_WANT_TARGET"
					die "Specify JAVA_PKG_WANT_TARGET"
				fi
			# otherwise determine a vm from dep string
			else
				debug-print "depend-java-query:  NV_DEPEND:	${JAVA_PKG_NV_DEPEND:-${DEPEND}}"
				GENTOO_VM="$(depend-java-query --get-vm "${JAVA_PKG_NV_DEPEND:-${DEPEND}}")"
				if [[ -z "${GENTOO_VM}" || "${GENTOO_VM}" == "None" ]]; then
					eerror "Unable to determine VM for building from dependencies:"
					echo "NV_DEPEND: ${JAVA_PKG_NV_DEPEND:-${DEPEND}}"
					die "Failed to determine VM for building."
				fi
			fi
			export GENTOO_VM
		# otherwise just make sure the current VM is sufficient
		else
			java-pkg_ensure-vm-version-sufficient
		fi
		debug-print "Using: $(java-config -f)"

		java-pkg_setup-vm

		export JAVA=$(java-config --java)
		export JAVAC=$(java-config --javac)
		JAVACFLAGS="$(java-pkg_javac-args)"
		if [[ ${?} != 0 ]]; then
			eerror "There was a problem determining JAVACFLAGS: ${JAVACFLAGS}"
			die "java-pkg_javac-args failed"
		fi
		[[ -n ${JAVACFLAGS_EXTRA} ]] && JAVACFLAGS="${JAVACFLAGS_EXTRA} ${JAVACFLAGS}"
		export JAVACFLAGS

		export JAVA_HOME="$(java-config -g JAVA_HOME)"
		export JDK_HOME=${JAVA_HOME}

		#TODO If you know a better solution let us know.
		java-pkg_append_ LD_LIBRARY_PATH "$(java-config -g LDPATH)"

		local tann="${T}/announced-vm"
		# With the hooks we should only get here once from pkg_setup but better safe than sorry
		# if people have for example modified eclasses some where
		if [[ -n "${JAVA_PKG_DEBUG}" ]] || [[ ! -f "${tann}" ]] ; then
			einfo "Using: $(java-config -f)"
			[[ ! -f "${tann}" ]] && touch "${tann}"
		fi

	else
		[[ -n "${JAVA_PKG_DEBUG}" ]] && ewarn "!!! This package inherits java-pkg but doesn't depend on a JDK. -bin or broken dependency!!!"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_die
#
# Enhanced die for Java packages, which displays some information that may be
# useful for debugging bugs on bugzilla.
# ------------------------------------------------------------------------------
#register_die_hook java-pkg_die
if ! has java-pkg_die ${EBUILD_DEATH_HOOKS}; then
	EBUILD_DEATH_HOOKS="${EBUILD_DEATH_HOOKS} java-pkg_die"
fi

java-pkg_die() {
	echo "!!! When you file a bug report, please include the following information:" >&2
	echo "GENTOO_VM=${GENTOO_VM}  CLASSPATH=\"${CLASSPATH}\" JAVA_HOME=\"${JAVA_HOME}\"" >&2
	echo "JAVACFLAGS=\"${JAVACFLAGS}\" COMPILER=\"${GENTOO_COMPILER}\"" >&2
	echo "and of course, the output of emerge --info" >&2
}


# TODO document
# List jars in the source directory, ${S}
java-pkg_jar-list() {
	if [[ -n "${JAVA_PKG_DEBUG}" ]]; then
		einfo "Linked Jars"
		find "${S}" -type l -name '*.jar' -print0 | xargs -0 -r -n 500 ls -ald | sed -e "s,${WORKDIR},\${WORKDIR},"
		einfo "Jars"
		find "${S}" -type f -name '*.jar' -print0 | xargs -0 -r -n 500 ls -ald | sed -e "s,${WORKDIR},\${WORKDIR},"
		einfo "Classes"
		find "${S}" -type f -name '*.class' -print0 | xargs -0 -r -n 500 ls -ald | sed -e "s,${WORKDIR},\${WORKDIR},"
	fi
}

# ------------------------------------------------------------------------------
# @internal-function java-pkg_verify-classes
#
# Verify that the classes were compiled for the right source / target. Dies if
# not.
# @param $1 (optional) - the file to check, otherwise checks whole ${D}
# ------------------------------------------------------------------------------
java-pkg_verify-classes() {
	#$(find ${D} -type f -name '*.jar' -o -name '*.class')

	local version_verify="/usr/bin/class-version-verify.py"

	if [[ ! -x "${version_verify}" ]]; then
		version_verify="/usr/$(get_libdir)/javatoolkit/bin/class-version-verify.py"
	fi

	if [[ ! -x "${version_verify}" ]]; then
		ewarn "Unable to perform class version checks as"
		ewarn "class-version-verify.py is unavailable"
		ewarn "Please install dev-java/javatoolkit."
		return
	fi

	local target=$(java-pkg_get-target)
	local result
	local log="${T}/class-version-verify.log"
	if [[ -n "${1}" ]]; then
		${version_verify} -v -t ${target} "${1}" > "${log}"
		result=$?
	else
		ebegin "Verifying java class versions (target: ${target})"
		${version_verify} -v -t ${target} -r "${D}" > "${log}"
		result=$?
		eend ${result}
	fi
	[[ -n ${JAVA_PKG_DEBUG} ]] && cat "${log}"
	if [[ ${result} != 0 ]]; then
		eerror "Incorrect bytecode version found"
		[[ -n "${1}" ]] && eerror "in file: ${1}"
		eerror "See ${log} for more details."
		die "Incorrect bytecode found"
	fi
}

# ----------------------------------------------------------------------------
# @internal-function java-pkg_ensure-dep
# Check that a package being used in jarfrom, getjars and getjar is contained
# within DEPEND or RDEPEND.
# @param $1 - empty - check both vars; "runtime" or "build" - check only
#	RDEPEND, resp. DEPEND
# @param $2 - Package name and slot.

java-pkg_ensure-dep() {
	debug-print-function ${FUNCNAME} $*

	local limit_to="${1}"
	local target_pkg="${2}"
	local dev_error=""

	# remove the version specification, which may include globbing (* and [123])
	local stripped_pkg=$(echo "${target_pkg}" | sed \
		's/-\([0-9*]*\(\[[0-9]*\]\)*\)*\(\.\([0-9*]*\(\[[0-9]*\]\)*\)*\)*$//')

	debug-print "Matching against: ${stripped_pkg}"

	if [[ ${limit_to} != runtime && ! ( "${DEPEND}" =~ "$stripped_pkg" ) ]]; then
		dev_error="The ebuild is attempting to use ${target_pkg} that is not"
		dev_error="${dev_error} declared in DEPEND."
		if is-java-strict; then
			eerror "${dev_error}"
			die "${dev_error}"
		elif [[ ${BASH_SUBSHELL} = 0 ]]; then
			eerror "${dev_error}"
			elog "Because you have this package installed the package will"
			elog "build without problems, but please report this to"
			elog "http://bugs.gentoo.org"
		fi
	fi

	if [[ ${limit_to} != build ]]; then
		if [[ ! ( ${RDEPEND} =~ "${stripped_pkg}" ) ]]; then
			if [[ ! ( ${PDEPEND} =~ "${stripped_pkg}" ) ]]; then
				dev_error="The ebuild is attempting to use ${target_pkg},"
				dev_error="${dev_error} without specifying --build-only, that is not declared in RDEPEND"
				dev_error="${dev_error} or PDEPEND."
				if is-java-strict; then
					eerror "${dev_error}"
					die "${dev_error}"
				elif [[ ${BASH_SUBSHELL} = 0 ]]; then
					eerror "${dev_error}"
					elog "The package will build without problems, but may fail to run"
					elog "if you don't have ${target_pkg} installed, so please report"
					elog "this to http://bugs.gentoo.org"
				fi
			fi
		fi
	fi
}

# ------------------------------------------------------------------------------
# @section-end internal
# ------------------------------------------------------------------------------

java-pkg_check-phase() {
	local phase=${1}
	local funcname=${FUNCNAME[1]}
	if [[ ${EBUILD_PHASE} != ${phase} ]]; then
		local msg="${funcname} used outside of src_${phase}"
		java-pkg_announce-qa-violation "${msg}"
	fi
}

java-pkg_check-versioned-jar() {
	local jar=${1}

	if [[ ${jar} =~ ${PV} ]]; then
		java-pkg_announce-qa-violation "installing versioned jar '${jar}'"
	fi
}

java-pkg_check-jikes() {
	if has jikes ${IUSE}; then
		java-pkg_announce-qa-violation "deprecated USE flag 'jikes' in IUSE"
	fi
}

java-pkg_announce-qa-violation() {
	local nodie
	if [[ ${1} == "--nodie" ]]; then
		nodie="true"
		shift
	fi
	echo "Java QA Notice: $@" >&2
	increment-qa-violations
	[[ -z "${nodie}" ]] && is-java-strict && die "${@}"
}

increment-qa-violations() {
	let "JAVA_PKG_QA_VIOLATIONS+=1"
	export JAVA_PKG_QA_VIOLATIONS
}

is-java-strict() {
	[[ -n ${JAVA_PKG_STRICT} ]]
	return $?
}


# ------------------------------------------------------------------------------
# @eclass-end
# ------------------------------------------------------------------------------
