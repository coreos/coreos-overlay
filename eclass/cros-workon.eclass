# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# @ECLASS: cros-workon.eclass
# @MAINTAINER:
# ChromiumOS Build Team
# @BUGREPORTS:
# Please report bugs via http://crosbug.com/new (with label Area-Build)
# @VCSURL: http://git.chromium.org/gitweb/?p=chromiumos/overlays/chromiumos-overlay.git;a=blob;f=eclass/@ECLASS@
# @BLURB: helper eclass for building ChromiumOS packages from git
# @DESCRIPTION:
# A lot of ChromiumOS packages (src/platform/ and src/third_party/) are
# managed in the same way.  You've got a git tree and you want to build
# it.  This automates a lot of that common stuff in one place.


# Array variables. All of the following variables can contain multiple items
# with the restriction being that all of them have to have either:
# - the same number of items globally
# - one item as default for all
# - no items as the cros-workon default
# The exception is CROS_WORKON_PROJECT which has to have all items specified.
ARRAY_VARIABLES=( CROS_WORKON_{SUBDIR,REPO,PROJECT,LOCALDIR,LOCALNAME,DESTDIR,COMMIT,TREE} )

# @ECLASS-VARIABLE: CROS_WORKON_SUBDIR
# @DESCRIPTION:
# Sub-directory which is added to create full source checkout path
: ${CROS_WORKON_SUBDIR:=}

# @ECLASS-VARIABLE: CROS_WORKON_REPO
# @DESCRIPTION:
# Git URL which is prefixed to CROS_WORKON_PROJECT
: ${CROS_WORKON_REPO:=https://chromium.googlesource.com}

# @ECLASS-VARIABLE: CROS_WORKON_PROJECT
# @DESCRIPTION:
# Git project name which is suffixed to CROS_WORKON_REPO
: ${CROS_WORKON_PROJECT:=${PN}}

# @ECLASS-VARIABLE: CROS_WORKON_LOCALDIR
# @DESCRIPTION:
# Repo checkout directory which is prefixed to CROS_WORKON_LOCALNAME
# Generally is either src/third_party or src/platform
: ${CROS_WORKON_LOCALDIR:=src/third_party}

# @ECLASS-VARIABLE: CROS_WORKON_LOCALNAME
# @DESCRIPTION:
# Directory name which is suffixed to CROS_WORKON_LOCALDIR
: ${CROS_WORKON_LOCALNAME:=${PN}}

# @ECLASS-VARIABLE: CROS_WORKON_DESTDIR
# @DESCRIPTION:
# Destination directory in ${WORKDIR} for checkout.
# Note that the default is ${S}, but is only referenced in src_unpack for
# ebuilds that would like to override it.
: ${CROS_WORKON_DESTDIR:=}

# @ECLASS-VARIABLE: CROS_WORKON_COMMIT
# @DESCRIPTION:
# Git commit to checkout to
: ${CROS_WORKON_COMMIT:=master}

# @ECLASS-VARIABLE: CROS_WORKON_TREE
# @DESCRIPTION:
# SHA1 of the contents of the repository. This is used for verifying the
# correctness of prebuilts. Unlike the commit hash, this SHA1 is unaffected
# by the history of the repository, or by commit messages.
: ${CROS_WORKON_TREE:=}

# Scalar variables. These variables modify the behaviour of the eclass.

# @ECLASS-VARIABLE: CROS_WORKON_SUBDIRS_TO_COPY
# @DESCRIPTION:
# Make cros-workon operate exclusively with the subtrees given by this array.
# NOTE: This only speeds up local_cp builds. Inplace/local_git builds are unaffected.
# It will also be disabled by using project arrays, rather than a single project.
: ${CROS_WORKON_SUBDIRS_TO_COPY:=/}

# @ECLASS-VARIABLE: CROS_WORKON_SUBDIRS_BLACKLIST
# @DESCRIPTION:
# Array of directories in the source tree to explicitly ignore and not even copy
# them over. This is intended, for example, for blocking infamous bloated and
# generated content that is unwanted during the build.
: ${CROS_WORKON_SUBDIRS_BLACKLIST:=}

# @ECLASS-VARIABLE: CROS_WORKON_SRCROOT
# @DESCRIPTION:
# Directory where chrome third party and platform sources are located (formerly CHROMEOS_ROOT)
: ${CROS_WORKON_SRCROOT:=}

# @ECLASS-VARIABLE: CROS_WORKON_INPLACE
# @DESCRIPTION:
# Build the sources in place. Don't copy them to a temp dir.
: ${CROS_WORKON_INPLACE:=}

# @ECLASS-VARIABLE: CROS_WORKON_USE_VCSID
# @DESCRIPTION:
# Export VCSID into the project
: ${CROS_WORKON_USE_VCSID:=}

# @ECLASS-VARIABLE: CROS_WORKON_GIT_SUFFIX
# @DESCRIPTION:
# The git eclass does not do locking on its repo.  That means
# multiple ebuilds that use the same git repo cannot safely be
# emerged at the same time.  Until we can get that sorted out,
# allow ebuilds that know they'll conflict to declare a unique
# path for storing the local clone.
: ${CROS_WORKON_GIT_SUFFIX:=}

# @ECLASS-VARIABLE: CROS_WORKON_OUTOFTREE_BUILD
# @DESCRIPTION:
# Do not copy the source tree to $S; instead set $S to the
# source tree and store compiled objects and build state
# in $WORKDIR.  The ebuild is responsible for ensuring
# the build output goes to $WORKDIR, e.g. setting
# O=${WORKDIR}/${P}/build/${board} when compiling the kernel.
: ${CROS_WORKON_OUTOFTREE_BUILD:=}

# @ECLASS-VARIABLE: CROS_WORKON_INCREMENTAL_BUILD
# @DESCRIPTION:
# If set to "1", store output objects in a location that is not wiped
# between emerges.  If disabled, objects will be written to ${WORKDIR}
# like normal.
: ${CROS_WORKON_INCREMENTAL_BUILD:=}

# @ECLASS-VARIABLE: CROS_WORKON_BLACKLIST
# @DESCRIPTION:
# If set to "1", the cros-workon uprev system on the bots will not automatically
# revbump your package when changes are made.  This is useful if you want more
# direct control over when updates to the source git repo make it into the
# ebuild, or if the git repo you're using is not part of the official manifest.
# e.g. If you set CROS_WORKON_REPO or EGIT_REPO_URI to an external (to Google)
# site, set this to "1".
: ${CROS_WORKON_BLACKLIST:=}

# Join the tree commits to produce a unique identifier
CROS_WORKON_TREE_COMPOSITE=$(IFS="_"; echo "${CROS_WORKON_TREE[*]}")
IUSE="cros_workon_tree_$CROS_WORKON_TREE_COMPOSITE profiling"

inherit git-2 flag-o-matic toolchain-funcs

# Sanitize all variables, autocomplete where necessary.
# This function possibly modifies all CROS_WORKON_ variables inplace. It also
# provides a global project_count variable which contains the number of
# projects.
array_vars_autocomplete() {
	# NOTE: This one variable has to have all values explicitly filled in.
	project_count=${#CROS_WORKON_PROJECT[@]}

	# No project_count is really bad.
	[ ${project_count} -eq 0 ] && die "Must have at least one CROS_WORKON_PROJECT"
	# For one project, defaults will suffice.
	[ ${project_count} -eq 1 ] && return

	[[ ${CROS_WORKON_OUTOFTREE_BUILD} == "1" ]] && die "Out of Tree Build not compatible with multi-project ebuilds"

	local count var
	for var in "${ARRAY_VARIABLES[@]}"; do
		eval count=\${#${var}\[@\]}
		if [[ ${count} -ne ${project_count} ]] && [[ ${count} -ne 1 ]]; then
			die "${var} has ${count} projects. ${project_count} or one default expected."
		fi
		# Invariably, ${project_count} is at least 2 here. All variables also either
		# have all items or the first serves as default (or isn't needed if
		# empty). By looking at the second item, determine if we need to
		# autocomplete.
		local i
		if [[ ${count} -ne ${project_count} ]]; then
			for (( i = 1; i < project_count; ++i )); do
				eval ${var}\[i\]=\${${var}\[0\]}
			done
		fi
		eval einfo "${var}: \${${var}[@]}"
	done
}

# Calculate path where code should be checked out.
# Result passed through global variable "path" to preserve proper array quoting.
get_paths() {
	local pathbase
	if [[ -n "${CROS_WORKON_SRCROOT}" ]]; then
		pathbase="${CROS_WORKON_SRCROOT}"
	elif [[ -n "${CHROMEOS_ROOT}" ]]; then
		pathbase="${CHROMEOS_ROOT}"
	else
		# HACK: Figure out the missing legacy path for now
		# this only happens in amd64 chroot with sudo emerge.
		pathbase="/mnt/host/source"
	fi

	path=()
	local pathelement i
	for (( i = 0; i < project_count; ++i )); do
		pathelement="${pathbase}/${CROS_WORKON_LOCALDIR[i]}"
		pathelement+="/${CROS_WORKON_LOCALNAME[i]}"
		if [[ -n "${CROS_WORKON_SUBDIR[i]}" ]]; then
			pathelement+="/${CROS_WORKON_SUBDIR[i]}"
		fi
		path+=( "${pathelement}" )
	done
}

local_copy_cp() {
	local src="${1}"
	local dst="${2}"
	einfo "Copying sources from ${src}"
	local blacklist=( "${CROS_WORKON_SUBDIR_BLACKLIST[@]/#/--exclude=}" )

	local sl
	for sl in "${CROS_WORKON_SUBDIRS_TO_COPY[@]}"; do
		if [[ -d "${src}/${sl}" ]]; then
			mkdir -p "${dst}/${sl}"
			rsync -a "${blacklist[@]}" "${src}/${sl}"/* "${dst}/${sl}" || \
				die "rsync -a ${blacklist[@]} ${src}/${sl}/* ${dst}/${sl}"
		fi
	done
}

symlink_in_place() {
	local src="${1}"
	local dst="${2}"
	einfo "Using experimental inplace build in ${src}."

	SBOX_TMP=":${SANDBOX_WRITE}:"

	if [ "${SBOX_TMP/:$CROS_WORKON_SRCROOT://}" == "${SBOX_TMP}" ]; then
		ewarn "For inplace build you need to modify the sandbox"
		ewarn "Set SANDBOX_WRITE=${CROS_WORKON_SRCROOT} in your env."
	fi

	ln -sf "${src}" "${dst}"
}

local_copy() {
	# Local vars used by all called functions.
	local src="${1}"
	local dst="${2}"

	# If we want to use git, and the source actually is a git repo
	if [ "${CROS_WORKON_INPLACE}" == "1" ]; then
		symlink_in_place "${src}" "${dst}"
	elif [ "${CROS_WORKON_OUTOFTREE_BUILD}" == "1" ]; then
		S="${src}"
	else
		local_copy_cp "${src}" "${dst}"
	fi
}

set_vcsid() {
	export VCSID="${PVR}-${1}"

	if [ "${CROS_WORKON_USE_VCSID}" = "1" ]; then
		append-flags -DVCSID=\\\"${VCSID}\\\"
		MAKEOPTS+=" VCSID=${VCSID}"
	fi
}

get_rev() {
	GIT_DIR="$1" git rev-parse HEAD
}

using_common_mk() {
	[[ -n $(find -H "${S}" -name common.mk -exec grep -l common-mk.git {} +) ]]
}

cros-workon_src_unpack() {
	local fetch_method # local|git

	# Sanity check.  We cannot have S set to WORKDIR because if/when we try
	# to check out repos, git will die if it tries to check out into a dir
	# that already exists.  Some packages might try this when out-of-tree
	# builds are enabled, and they'll work fine most of the time because
	# they'll be using a full manifest and will just re-use the existing
	# checkout in src/platform/*.  But if the code detects that it has to
	# make its own checkout, things fall apart.  For out-of-tree builds,
	# the initial $S doesn't even matter because it resets it below to the
	# repo in src/platform/.
	if [[ ${S} == "${WORKDIR}" ]]; then
		die "Sorry, but \$S cannot be set to \$WORKDIR"
	fi

	# Set the default of CROS_WORKON_DESTDIR. This is done here because S is
	# sometimes overridden in ebuilds and we cannot rely on the global state
	# (and therefore ordering of eclass inherits and local ebuild overrides).
	: ${CROS_WORKON_DESTDIR:=${S}}

	# Fix array variables
	array_vars_autocomplete

	if [[ "${PV}" == "9999" ]]; then
		# Live packages
		fetch_method=local
	else
		fetch_method=git
	fi

	# Hack
	# TODO(msb): remove once we've resolved the include path issue
	# http://groups.google.com/a/chromium.org/group/chromium-os-dev/browse_thread/thread/5e85f28f551eeda/3ae57db97ae327ae
	local p i
	for p in "${CROS_WORKON_LOCALNAME[@]/#/${WORKDIR}/}"; do
		ln -s "${S}" "${p}" &> /dev/null
	done

	local repo=( "${CROS_WORKON_REPO[@]}" )
	local project=( "${CROS_WORKON_PROJECT[@]}" )
	local destdir=( "${CROS_WORKON_DESTDIR[@]}" )
	get_paths

	# Automatically build out-of-tree for common.mk packages.
	# TODO(vapier): Enable this once all common.mk packages have converted.
	#if [[ -e ${path}/common.mk ]] ; then
	#	: ${CROS_WORKON_OUTOFTREE_BUILD:=1}
	#fi

	if [[ ${fetch_method} == "git" && ${CROS_WORKON_OUTOFTREE_BUILD} == "1" ]] ; then
		# See if the local repo exists, is unmodified, and is checked out to
		# the right rev.  This will be the common case, so support it to make
		# builds a bit faster.
		if [[ -d ${path} ]] ; then
			if [[ ${CROS_WORKON_COMMIT} == "$(get_rev "${path}/.git")" ]] ; then
				local changes=$(
					cd "${path}"
					# Needed as `git status` likes to grab a repo lock.
					addpredict "${PWD}"
					# Ignore untracked files as they (should) be ignored by the build too.
					git status --porcelain | grep -v '^[?][?]'
				)
				if [[ -z ${changes} ]] ; then
					fetch_method=local
				else
					# Assume that if the dev has changes, they want it that way.
					: #ewarn "${path} contains changes"
				fi
			else
				ewarn "${path} is not at rev ${CROS_WORKON_COMMIT}"
			fi
		else
			# This will hit minilayout users a lot, and rarely non-minilayout
			# users.  So don't bother warning here.
			: #ewarn "${path} does not exist"
		fi
	fi

	if [[ "${fetch_method}" == "git" ]] ; then
		all_local() {
			local p
			for p in "${path[@]}"; do
				[[ -d ${p} ]] || return 1
			done
			return 0
		}

		local fetched=0
		if all_local; then
			for (( i = 0; i < project_count; ++i )); do
				# Looks like we already have a local copy of all repositories.
				# Let's use these and checkout ${CROS_WORKON_COMMIT}.
				#  -s: For speed, share objects between ${path} and ${S}.
				#  -n: Don't checkout any files from the repository yet. We'll
				#      checkout the source separately.
				#
				# We don't use git clone to checkout the source because the -b
				# option for clone defaults to HEAD if it can't find the
				# revision you requested. On the other hand, git checkout fails
				# if it can't find the revision you requested, so we use that
				# instead.

				# Destination directory. If we have one project, it's simply
				# ${CROS_WORKON_DESTDIR}. More projects either specify an array or go to
				# ${S}/${project}.

				if [[ "${CROS_WORKON_COMMIT[i]}" == "master" ]]; then
					# Since we don't have a CROS_WORKON_COMMIT revision specified,
					# we don't know what revision the ebuild wants. Let's take the
					# version of the code that the user has checked out.
					#
					# This almost replicates the pre-cros-workon behavior, where
					# the code you had in your source tree was used to build
					# things. One difference here, however, is that only committed
					# changes are included.
					#
					# TODO(davidjames): We should fix the preflight buildbot to
					# specify CROS_WORKON_COMMIT for all ebuilds, and update this
					# code path to fail and explain the problem.
					git clone -s "${path[i]}" "${destdir[i]}" || \
						die "Can't clone ${path[i]}."
						: $(( ++fetched ))
				else
					git clone -sn "${path[i]}" "${destdir[i]}" || \
						die "Can't clone ${path[i]}."
					if ! ( cd ${destdir[i]} && git checkout -q ${CROS_WORKON_COMMIT[i]} ) ; then
						ewarn "Cannot run git checkout ${CROS_WORKON_COMMIT[i]} in ${destdir[i]}."
						ewarn "Is ${path[i]} up to date? Try running repo sync."
						rm -rf "${destdir[i]}/.git"
					else
						: $(( ++fetched ))
					fi
				fi
			done
			if [[ ${fetched} -eq ${project_count} ]]; then
				# TODO: Id of all repos?
				set_vcsid "$(get_rev "${path[0]}/.git")"
				return
			else
				ewarn "Falling back to git.eclass..."
			fi
		fi

		# Since we have no indication of being on a branch, it would
		# default to 'master', and that might not contain our commit, as
		# minilayout can have git checkouts independent of the repo tree.
		# Hack around this by using empty branch. This will cause git fetch to
		# pull all branches instead. Note that the branch has to be a space,
		# rather than empty, for this trick to work.
		EGIT_BRANCH=" "
		for (( i = 0; i < project_count; ++i )); do
			EGIT_REPO_URI="${repo[i]}/${project[i]}.git"
			EGIT_PROJECT="${project[i]}${CROS_WORKON_GIT_SUFFIX}"
			EGIT_SOURCEDIR="${destdir[i]}"
			EGIT_COMMIT="${CROS_WORKON_COMMIT[i]}"
			# Clones to /var, copies src tree to the /build/<board>/tmp.
			# Make sure git-2 does not run `unpack` for us automatically.
			# The normal cros-workon flow above doesn't do it, so don't
			# let git-2 do it either.  http://crosbug.com/38342
			EGIT_NOUNPACK=true git-2_src_unpack
			# TODO(zbehan): Support multiple projects for vcsid?
		done
		set_vcsid "${CROS_WORKON_COMMIT[0]}"
		return
	fi

	einfo "Using local source dir(s): ${path[*]}"

	# Clone from the git host + repository path specified by
	# CROS_WORKON_REPO + CROS_WORKON_PROJECT. Checkout source from
	# the branch specified by CROS_WORKON_COMMIT into the workspace path.
	# If the repository exists just punt and let it be copied off for build.
	if [[ "${fetch_method}" == "local" && ! -d ${path} ]] ; then
		ewarn "Sources are missing in ${path}"
		ewarn "You need to cros_workon and repo sync your project. For example if you are working on the shill ebuild and repository:"
		ewarn "cros_workon start --board=x86-generic shill"
		ewarn "repo sync shill"
	fi

	einfo "path: ${path[*]}"
	einfo "destdir: ${destdir[*]}"
	# Copy source tree to /build/<board>/tmp for building
	for (( i = 0; i < project_count; ++i )); do
		local_copy "${path[i]}" "${destdir[i]}" || \
			die "Cannot create a local copy"
		set_vcsid "$(get_rev "${path[0]}/.git")"
	done
}

cros-workon_get_build_dir() {
	local dir
	# strip subslot, for EAPI=5
	local slot="${SLOT%/*}"
	if [[ ${CROS_WORKON_INCREMENTAL_BUILD} == "1" ]]; then
		dir="${SYSROOT}/var/cache/portage/${CATEGORY}/${PN}"
		[[ ${slot:-0} != "0" ]] && dir+=":${slot}"
	else
		dir="${WORKDIR}/build"
	fi
	echo "${dir}"
}

cros-workon_pkg_setup() {
	if [[ ${CROS_WORKON_INCREMENTAL_BUILD} == "1" ]]; then
		local out=$(cros-workon_get_build_dir)
		addwrite "${out}"
		mkdir -p -m 755 "${out}" || die
		chown ${PORTAGE_USERNAME:-portage}:${PORTAGE_GRPNAME:-portage} "${out}" "${out%/*}" || die
	fi
}

cros-workon_src_prepare() {
	local out="$(cros-workon_get_build_dir)"
	[[ ${CROS_WORKON_INCREMENTAL_BUILD} != "1" ]] && mkdir -p "${out}"

	if using_common_mk ; then
		: ${OUT=${out}}
		export OUT
	fi
}

cros-workon_src_configure() {
	if using_common_mk ; then
		# We somewhat overshoot here, but it isn't harmful,
		# and catches all the packages we care about.
		tc-export CC CXX AR RANLIB LD NM PKG_CONFIG

		# Portage takes care of this for us.
		export SPLITDEBUG=0
		export MODE=$(usex profiling profiling opt)
		if [[ $(type -t cros-debug-add-NDEBUG) == "function" ]] ; then
			# Only run this if we've inherited cros-debug.eclass.
			cros-debug-add-NDEBUG
		fi
		if [[ ${LIBCHROME_VERS:+set} == "set" ]] ; then
			# For packages that use libchromeos, set it up automatically.
			export BASE_VER=${LIBCHROME_VERS}
		fi
	else
		default
	fi
}

cw_emake() {
	local dir=$(cros-workon_get_build_dir)

	# Clean up a previous build dir if it exists.  Use sudo in case some
	# files happened to be owned by root or are otherwise marked a-w.
	# Grep out the sandbox related preload error to avoid confusing devs.
	(sudo rm -rf "${dir}%failed" 2>&1 | \
		grep -v 'LD_PRELOAD cannot be preloaded: ignored') &

	if ! nonfatal emake "$@" ; then
		# If things failed, move the incremental dir out of the way --
		# we don't know why exactly it failed as it could be due to
		# corruption.  Don't throw it away immediately in case the the
		# developer wants to poke around.
		# http://crosbug.com/35958
		if [[ ${CROS_WORKON_INCREMENTAL_BUILD} == "1" ]] ; then
			if [[ $(hostname -d) == "golo.chromium.org" ]] ; then
				eerror "The build failed.  Output has been retained at:"
				eerror "  ${dir}%failed/"
				eerror "It will be cleaned up automatically next emerge."
				wait # wait for the `rm` to finish.
				mv "${dir}" "${dir}%failed"
			else
				ewarn "If this failure is due to build-dir corruption, run:"
				ewarn "  sudo rm -rf '${dir}'"
			fi
		fi
		die "command: emake $*"
	fi
}

cros-workon_src_compile() {
	if using_common_mk ; then
		cw_emake
	else
		default
	fi
}

cros-workon_src_test() {
	if using_common_mk ; then
		emake \
			VALGRIND=$(use_if_iuse valgrind && echo 1) \
			tests
	else
		default
	fi
}

cros-workon_src_install() {
	# common.mk supports coverage analysis, but only generates data when
	# the tests have been run as part of the build process. Thus this code
	# needs to test of the analysis output is present before trying to
	# install it.
	if using_common_mk ; then
		if use profiling; then
			LCOV_DIR=$(find "${WORKDIR}" -name "lcov-html")
			if [[ $(echo "${LCOV_DIR}" | wc -l) -gt 1 ]] ; then
				die "More then one instance of lcov-html " \
				    "found! The instances are ${LCOV_DIR}. " \
				    "It is unclear which version to use, " \
				    "failing install."
			fi
			if [[ -d "${LCOV_DIR}" ]] ; then
				local dir="${PN}"
				# strip subslot, for EAPI=5
				local slot="${SLOT%/*}"
				[[ ${slot} != "0" ]] && dir+=":${slot}"
				insinto "/usr/share/profiling/${dir}/lcov"
				doins -r "${LCOV_DIR}"/*
			fi
		fi
	else
		default
	fi
}

cros-workon_pkg_info() {
	print_quoted_array() { printf '"%s"\n' "$@"; }

	array_vars_autocomplete > /dev/null
	get_paths
	CROS_WORKON_SRCDIR=("${path[@]}")

	local val var
	for var in CROS_WORKON_SRCDIR CROS_WORKON_PROJECT ; do
		eval val=(\"\${${var}\[@\]}\")
		echo ${var}=\($(print_quoted_array "${val[@]}")\)
	done
}

EXPORT_FUNCTIONS pkg_setup src_unpack pkg_info
